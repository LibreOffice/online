/* -*- js-indent-level: 8 -*- */
/* global loleafletLogging */
/*eslint indent: [error, "tab", { "outerIIFEBody": 0 }]*/
(function (global) {

// If not debug, don't print anything on the console
// except in tile debug mode (Ctrl-Shift-Alt-d)
console.log2 = console.log;
if (loleafletLogging !== 'true') {
	var methods = ['warn', 'info', 'debug', 'trace', 'log', 'assert', 'time', 'timeEnd'];
	for (var i = 0; i < methods.length; i++) {
		console[methods[i]] = function() {};
	}
}

// fix jquery-ui
// var jQuery = require('jquery');
global.require = function (path) {
	if (path=='jquery') {
		return global.jQuery;
	}
};

global.getParameterByName = function (name) {
	name = name.replace(/[\[]/, '\\[').replace(/[\]]/, '\\]');
	var regex = new RegExp('[\\?&]' + name + '=([^&#]*)');
	var results = regex.exec(location.search);
	return results === null ? '' : results[1].replace(/\+/g, ' ');
};

global._ = function (string) {
	// In the mobile app case we can't use the stuff from l10n-for-node, as that assumes HTTP.
	if (window.ThisIsTheiOSApp) {
		// We use another approach just for iOS for now.
		if (window.LOCALIZATIONS.hasOwnProperty(string)) {
			// window.webkit.messageHandlers.debug.postMessage('_(' + string + '): YES: ' + window.LOCALIZATIONS[string]);
			var result = window.LOCALIZATIONS[string];
			if (window.LANG === 'de-CH') {
				result = result.replace(/ß/g, 'ss');
			}
			return result;
		} else {
			// window.webkit.messageHandlers.debug.postMessage('_(' + string + '): NO');
			return string;
		}
	} else if (window.ThisIsAMobileApp) {
		// And bail out without translations on other mobile platforms.
		return string;
	} else {
		return string.toLocaleString();
	}
};

var docParams, wopiParams;
var filePath = global.getParameterByName('file_path');
var timestamp = global.getParameterByName('timestamp');
var permission = global.getParameterByName('permission') || 'edit';
var lang = global.getParameterByName('lang');
var wopiSrc = global.getParameterByName('WOPISrc');

if (wopiSrc != '') {
	global.docURL = decodeURIComponent(wopiSrc);
	wopiSrc = '?WOPISrc=' + wopiSrc + '&compat=/ws';
	if (global.accessToken !== '') {
		wopiParams = { 'access_token': global.accessToken, 'access_token_ttl': global.accessTokenTTL };
	}
	else if (global.accessHeader !== '') {
		wopiParams = { 'access_header': global.accessHeader };
	}

	wopiParams['permission'] = permission;
	docParams = Object.keys(wopiParams).map(function(key) {
		return encodeURIComponent(key) + '=' + encodeURIComponent(wopiParams[key])
	}).join('&');
} else {
	global.docURL = filePath;
	docParams = 'permission=' + permission;
}

var websocketURI = global.host + global.serviceRoot + '/lool/' + encodeURIComponent(global.docURL + (docParams ? '?' + docParams : '')) + '/ws' + wopiSrc;

try {
	global.socket = new WebSocket(websocketURI);
} catch (err) {
	console.log(err);
}

global.queueMsg = [];
if (global.socket && global.socket.readyState !== 3) {
	global.socket.onopen = function () {
		if (global.socket.readyState === 1) {
			var ProtocolVersionNumber = '0.1';
			var msg = 'load url=' + encodeURIComponent(global.docURL);
			global.socket.send('loolclient ' + ProtocolVersionNumber);
			if (timestamp) {
				msg += ' timestamp=' + timestamp;
			}
			if (lang) {
				msg += ' lang=' + lang;
			}
			// renderingOptions?
			global.socket.send(msg);
		}
	}

	global.socket.onerror = function (event) {
		console.log(event);
	}

	global.socket.onclose = function (event) {
		console.log(event);
	}

	global.socket.onmessage = function (event) {
		if (global.L && global.socket instanceof global.L.Socket) {
			global.socket._onMessage(event);
		} else {
			global.queueMsg.push(event.data);
		}
	}

	global.socket.binaryType = 'arraybuffer';
}
}(window));
