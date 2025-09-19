function $(id) {
	return document.getElementById(id);
}

function open_files() {
	return new Promise((resolve, reject) => {
		let input = document.createElement('input');
		input.type = 'file';
		input.onchange = () => {
			resolve(input.files);
		}
		input.click();
	});
}

async function load_text_file() {
	let files = await open_files();
	return await files[0].text();
}

async function load_raw_file() {
	let files = await open_files();
	return await files[0].arrayBuffer();
}

function save_raw_file(name, content) {
	let a = document.createElement('a');
	a.download = name;
	a.href = "data:application/octet-stream;base64," + btoa(content);
	a.click();
}

function clone(obj) {
	return JSON.parse(JSON.stringify(obj));
}

let global_scope;
var myApp = angular.module('MyApp', []).controller('PCB2Gcode_Controller', function($scope, $http, $interval) {
	// For debugging
	global_scope = $scope;

	$scope.yaml = null;
	$scope.conf  = { inputs: [] };
	$scope.step1 = { enabled: true  };
	$scope.step2 = { enabled: false };

	$scope.presets = {
		"": "Custom profile",
		"BUNGARD-basic.p2g": "BUNGARD-basic",
		"BUNGARD-full.p2g": "BUNGARD-full",
		"BUNGARD-voronoi-basic.p2g": "BUNGARD-voronoi-basic",
		"BUNGARD-voronoi-full.p2g": "BUNGARD-voronoi-full",
		"BUNGARD-voronoi-multitool.p2g": "BUNGARD-voronoi-multitool",
		"CNC3018-basic.p2g": "CNC3018-basic",
		"CNC3018-full.p2g": "CNC3018-full",
		"CNC3018-voronoi-basic.p2g": "CNC3018-voronoi-basic",
		"CNC3018-voronoi-full.p2g": "CNC3018-voronoi-full",
		"CNC3018-voronoi-multitool.p2g": "CNC3018-voronoi-multitool"
	};

	function has_valid_inputs() {
		if (!$scope.conf || !$scope.conf.inputs)
			return false;

		for (let i in $scope.conf.inputs)
			if ($scope.conf.inputs[i]) return true;

		return false;
	}

	function reset_to_pre_run() {
		$scope.step3 = { enabled: has_valid_inputs() };
		$scope.step4 = { enabled: false };
		$scope.results = {};
		$scope.logs="";
	}
	reset_to_pre_run();

	function log_push(text) {
		$scope.logs = ($scope.logs + text).replaceAll("\r\n","\n").replaceAll(/\n[^\n]*\r/g,"\n").replaceAll("\033[K","");
		try { $scope.$apply(); }
		catch (e) {}
		window.scrollBy({top: 1000});
	};

	$scope.classes_for_input = key => {
		return {
			"w3-card ":  true,
			"w3-button": true,
			"w3-yellow": $scope.conf && $scope.conf.inputs[key] == null,
			"w3-green":  $scope.conf && $scope.conf.inputs[key] != null,
		};
	};

	$scope.set_config = yaml => {
		$scope.yaml = yaml;
		let conf = jsyaml.load($scope.yaml);

		// Discard file names on the configuration.
		// If user loaded files previously, try keeping them.
		for (let key in conf.inputs)
			conf.inputs[key] = $scope.conf.inputs[key] || null;

		$scope.conf = conf;

		$scope.step2.enabled = true;
	};

	$scope.load_config = async key => {
		let yaml = "";
		if (key == "") {
			yaml = await load_text_file();
		} else {
			let response = await fetch("presets/"+key);
			let ab = await response.arrayBuffer();
			yaml = new TextDecoder().decode(ab);
		}
		$scope.set_config(yaml);
		reset_to_pre_run();
		$scope.$apply();
	};
	
	$scope.load_input = key => {
		load_text_file().then(data => {
			$scope.conf.inputs[key] = data;
			reset_to_pre_run();
			$scope.$apply();
		});
	};
	
	$scope.start_processing = () => {
		$scope.conf["rp2g"] = { output: "zip" };

		reset_to_pre_run();
		let url = window.location.protocol == "https:" ? "wss://" : "ws://";
		url += window.location.host;
		url += "/";

		let conf = clone($scope.conf);
		for (let i in conf.inputs)
			if (conf.inputs[i] == null) {
				console.log(`Discard empty layer ${i}...`)
				delete conf.inputs[i];
			}

		function push_result(name, data) {
			console.log(`Result ${name}, ${data.length} bytes.`)
			$scope.results[name] = data;
			$scope.step4.enabled = true;
			$scope.$apply();
		}
		function on_close() {
			push_result("log.txt", $scope.logs);
		}

		rp2g(conf, url, push_result, log_push, console.error, on_close);
	};

	$scope.save_result = key => {
		save_raw_file(key, $scope.results[key]);
	}
})
