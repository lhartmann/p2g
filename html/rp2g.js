function rp2g(conf, server_address, on_file, on_text, on_error, on_close) {
	conf = new TextEncoder().encode(JSON.stringify(conf));

	on_text  = on_text  || console.log;
	on_error = on_error || console.error;
	on_close = on_close || (()=>{});

	// Start the connection to the WebSocket server at echo.websocket.org
	on_text("Connecting to server...\n");
	const ws = new WebSocket(server_address);
	ws.binaryType = "arraybuffer";

	// Register event listeners for the open, close, and message events
	ws.onopen = () => {
		// Send a message over the WebSocket to the server
		on_text("Sending work (" + conf.length + " bytes)...\n");
		ws.send(conf);
	};
	ws.onmessage = async message =>  {
		// Text messages are logs to be printed.
		if (typeof message.data === "string") {
			on_text(message.data);
			return;
		}
		
		// Binary data should be the result.
		let result = JSON.parse(new TextDecoder().decode(message.data));
		let op = new Date().toISOString();
		for (let file in result) {
			on_text("Received [" + file + "]...\n");
			on_file(file, atob(result[file]));
		}
		on_text("Done.\n");
		ws.close();
	};
	ws.onerror = (err) => on_error(err);
	ws.onclose = () => {
		on_text("Server connection closed.\n");
		on_close();
	}
}
