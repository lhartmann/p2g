#! /usr/bin/env -S deno run --allow-net --allow-read --allow-write --allow-run
import { deflate, inflate  } from "jsr:@deno-library/compress";
import * as yaml from "jsr:@std/yaml";
import { encodeBase64, decodeBase64 } from "jsr:@std/encoding";

let queue_busy = false;
let queue : any = [];
async function queue_poll() {
	if (queue_busy || queue.length==0) return;

	// Pop front of queue and run to completion
	let context = queue.splice(0,1)[0];
	queue_busy = true;
	try {
		await run_context(context);
	}
	catch(err) { console.error(err); }
	queue_busy = false;

	// Prevent memory leaks from self-references
	// for (let i in context)
	// 	delete context[i];

	// Start next job, but don't wait
	queue_poll();
}

async function queue_notify() {
	console.log(`Queue size: ${queue.length}, worker ${queue_busy ? "" : "in"}active.`);
	for (let i in queue) {
		queue[i].logc(`You are queued in position ${i} of ${queue.length}...`);
	}
}
setInterval(queue_notify, 5000);

//
function pack(data : any) : Uint8Array {
	return (new TextEncoder().encode(JSON.stringify(data)));
	return deflate(new TextEncoder().encode(JSON.stringify(data)));
}
function unpack(data : Uint8Array) : any {
	return JSON.parse(new TextDecoder().decode((data)));
	return JSON.parse(new TextDecoder().decode(inflate(data)));
}

// Returns a map of pseudo-path = real-path
async function create_packing_list(ctx: any) : Promise<any> {
	let list : any = {};
	list["config.p2g"] = ctx.rootdir + "/config.p2g";
	for (let o of ctx.config.outputs)
		list[o.file] = ctx.rootdir + '/' + o.file;
	if (ctx.config.debug) {
		for await (const dirEntry of Deno.readDir(ctx.rootdir + '/p2g-debug-out/')) {
			list["debug-" + dirEntry.name] = ctx.rootdir + '/p2g-debug-out/' + dirEntry.name;
		}
	}
	return list;con
}

async function pack_list(ctx : any, packing_list : any) : Promise<any> {
	ctx.log("Packing outputs...");
	for (let i in packing_list) try {
		ctx.log(`  ${packing_list[i]}`);
		packing_list[i] = encodeBase64(await Deno.readFile(packing_list[i]));
	} catch (err) {
		// Ignore, empty files are not be generated at all.
		ctx.log(`    Empty or missing, discarded.`);
		delete packing_list[i];
	}
	return packing_list;
}

async function pack_list_zip(ctx : any, packing_list : any) : Promise<any> {
	ctx.log("Packing outputs as zip file...");

	ctx.log("  Compressing...");
//	let list : any = [];
	let list : string[] = [ "zip", "output.zip" ];
	for (let f in packing_list) {
		list.push(packing_list[f].replace(ctx.rootdir+"/", ""));
	}
	console.log(list);
//	await compress(list, ctx.rootdir + "/output.zip")
	let P = Deno.run({
		"cmd": list,
		"cwd": ctx.rootdir
	});
	await P.status();

	ctx.log("  Reading...");
	packing_list = { "output.zip": encodeBase64(await Deno.readFile(ctx.rootdir + "/output.zip")) };

	return packing_list;
}

function sanityze_output_file_names(ctx: any) : void {
	ctx.log("Sanityzing output file names...");
	let counter = 0;
	for (let o of ctx.config.outputs) {
		// Filenames containing / or \ will be sanitized.
//		if (file.indexOf('/') >= 0 || file.indexOf('\\') >= 0) {
		if (!/^[a-zA-Z0-9][a-zA-Z0-9\-\. _]*$/.test(o.file)) {
			let file = `output_${counter++}.gcode`;
			ctx.log(`  ${o.file} => ${file}`);
			o.file = file;
		}
	}
}

async function collect_and_send_outputs(ctx : any) : Promise<void> {
	let packing_list = await create_packing_list(ctx);
	if (ctx.config.rp2g && ctx.config.rp2g.output == "zip")
		packing_list = await pack_list_zip(ctx, packing_list);
	else
		packing_list = await pack_list(ctx, packing_list);

	packing_list = pack(packing_list);

	ctx.log("Downloading results (" + packing_list.length + " bytes)...");
	ctx.send(packing_list)
}

async function create_context(sock: WebSocket, config: any) : Promise<any> {
	let ctx = {
		config: config,
		sock: sock,

		// Temporary storage
		rootdir: await Deno.makeTempDir({prefix: "rp2g-"}),

		// Shortcut, send to client
		send: (data:any) => ctx.sock.send(data),

		// Server-side log
		logs: (data:any) => console.log(`[${ctx.rootdir}] ${data}`),
		// Clinet-side log
		logc: (data:any) => sock.send(`${data}\n`),
		// Shared log
		log:  (data:any) => { ctx.logs(data); ctx.logc(data); },
	};
	return ctx;
}

async function run_context(ctx: any) {
	let P : any = false;
	try {
		let counter = 0;
		
		ctx.log("Server received job.");
		
		if (ctx.debug) {
			ctx.log("Debug output enabled.");
			Deno.mkdir(ctx.rootdir + '/p2g-debug-out');
		}
		
		// Extract files
		ctx.log("Extracting input files...");
		for (let layer in ctx.config.inputs) try {
			const file = `input_${counter++}.gbr`;
//			const content = decodeBase64(ctx.inputs[layer]);
			const content = ctx.config.inputs[layer];
			ctx.log(`Write ${layer} layer to ${file}...`);
			await Deno.writeTextFile(ctx.rootdir + '/' + file, content);
			ctx.config.inputs[layer] = file;
		} catch (err) {
			ctx.log(`Error ${err} exporting layer ${layer}, discarding.`);
			delete ctx.config.inputs[layer];
		}

		// Rewrite output file names for safety
		sanityze_output_file_names(ctx);
		
		// Write the confguration file
		ctx.log("Writing job configuration...");
		await Deno.writeTextFile(ctx.rootdir + '/config.p2g', yaml.stringify(ctx.config));
		
		// Run the job
		ctx.log("Started processing...");
		P = Deno.run({
			cmd: ["p2g", "config.p2g"],
			cwd: ctx.rootdir,
			stderr: 'piped'
		});
		
		// Forward stderr to client
		let data = new Uint8Array(512);
		while (true) {
			let n = await P.stderr.read(data);
			if (!n) break;
			ctx.send(new TextDecoder().decode(data.slice(0,n)));
		}
		
		// Get status
		let st = await P.status();
		P = false;
		ctx.log("Processing completed.");
		if (st.success) {
			ctx.log("gcode created successfully.");
			await collect_and_send_outputs(ctx);
		} else {
			ctx.log("Killed!");
		}
	} catch (err) {
		ctx.log("Error:", err);
		
		if (P)
			P.kill(9);
	}
	
	// Remove temp folder
	ctx.log("Cleaning...");
	await Deno.remove(ctx.rootdir, {recursive: true});
	
	// All done
	ctx.log("Completed.");
	if (!ctx.sock.isClosed)
		ctx.sock.close();
}

async function handleWs(sock: WebSocket) {
	console.log("socket connected!");

	sock.addEventListener("message", async (event) => {
		try {
			if (event.data instanceof ArrayBuffer) {
				console.log("Config received as packed message.");
				queue.push(await create_context(sock, unpack(event.data)));
			} else if (typeof event.data === "string") {
				console.log("Config received as JSON message.");
				queue.push(await create_context(sock, JSON.parse(event.data)));
			} else {
				console.log("Unknown message type:", event.data);
			}
			queue_poll();
		} catch (err) {
			console.error("failed to handle message:", err);
			sock.close(1011, "Internal error").catch(console.error);
		}
	});

	sock.addEventListener("close", (event) => {
		console.log("ws:Close", event.code, event.reason);
	});

	sock.addEventListener("error", (err) => {
		console.error("WebSocket error:", err);
	});
}

// Webroot
let args : any = {
	port: 8644,
	webroot: "."
}
for (let arg of Deno.args)
	for (let a in args)
		if (arg.startsWith(`--${a}=`))
			args[a] = arg.slice(`--${a}=`.length);
args.webroot = await Deno.realPath(args.webroot);

/** websocket echo server */
console.log(`websocket server is running on :${args.port}`);
Deno.serve({ port: args.port, hostname: "0.0.0.0" }, async (req) => {
	const { headers } = req;

	if (headers.get("upgrade") === "websocket") {
		try {
			const { socket, response } = Deno.upgradeWebSocket(req);
			handleWs(socket);
			return response; // return handshake response
		} catch (err) {
			console.error("failed to accept websocket:", err);
			return new Response("Bad Request", { status: 400 });
		}
	}

	if (args.webroot.length) {
		let pathname = new URL(req.url).pathname;
		let url = args.webroot + pathname;
		if (pathname === "/") {
			url = args.webroot + "/index.html";
		}
		try {
			url = await Deno.realPath(url);
			console.log("URL...:", url);
			console.log("ROOT..:", args.webroot);
			if (!url.startsWith(args.webroot)) {
				return new Response("Forbidden", { status: 403 });
			} else {
				return new Response(await Deno.readTextFile(url), {
					status: 200,
					headers: { "content-type": "text/html" },
				});
			}
		} catch (_err) {
			return new Response("Not found", { status: 404 });
		}
	}

	return new Response("Not found", { status: 404 });
});
