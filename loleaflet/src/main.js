/* -*- js-indent-level: 8 -*- */
/* global errorMessages getParameterByName accessToken accessTokenTTL accessHeader vex host */
/* global serviceRoot idleTimeoutSecs outOfFocusTimeoutSecs setupToolbar*/
/*eslint indent: [error, "tab", { "outerIIFEBody": 0 }]*/

// Hack to expose w2utils and w2ui as globals.
// The w2ui code just *assumes* the global namespace, and that technique
// breaks when bundling.
import { w2utils, w2ui } from '../js/w2ui-1.5.rc1.js';
window.w2utils = w2utils;
window.w2ui = w2ui;

var wopiParams;
var wopiSrc = getParameterByName('WOPISrc');

if (wopiSrc !== '' && accessToken !== '') {
	wopiParams = { 'access_token': accessToken, 'access_token_ttl': accessTokenTTL };
}
else if (wopiSrc !== '' && accessHeader !== '') {
	wopiParams = { 'access_header': accessHeader };
}

var filePath = getParameterByName('file_path');
var permission = getParameterByName('permission') || 'edit';
var timestamp = getParameterByName('timestamp');
// Shows close button if non-zero value provided
var closebutton = getParameterByName('closebutton');
// Shows revision history file menu option
var revHistoryEnabled = getParameterByName('revisionhistory');
// Should the document go inactive or not
var alwaysActive = getParameterByName('alwaysactive');
// Loleaflet Debug mode
var debugMode = getParameterByName('debug');
if (wopiSrc === '' && filePath === '' && !window.ThisIsAMobileApp) {
	vex.dialog.alert(errorMessages.wrongwopisrc);
}
if (host === '' && !window.ThisIsAMobileApp) {
	vex.dialog.alert(errorMessages.emptyhosturl);
}

// loleaflet.js accesses these globals FIXME!!!
// TODO: Get rid of these globals
window.closebutton = closebutton;
window.revHistoryEnabled = revHistoryEnabled;
var docURL, docParams;
var isWopi = false;
if (wopiSrc != '') {
	docURL = decodeURIComponent(wopiSrc);
	docParams = wopiParams;
	isWopi = true;
} else {
	docURL = filePath;
	docParams = {};
}

var notWopiButIframe = getParameterByName('NotWOPIButIframe') != '';

var map = L.map('map', {
	server: host,
	doc: docURL,
	serviceRoot: serviceRoot,
	docParams: docParams,
	permission: permission,
	timestamp: timestamp,
	documentContainer: 'document-container',
	debug: debugMode,
	// the wopi and wopiSrc properties are in sync: false/true : empty/non-empty
	wopi: isWopi,
	wopiSrc: wopiSrc,
	notWopiButIframe: notWopiButIframe,
	alwaysActive: alwaysActive,
	idleTimeoutSecs: idleTimeoutSecs,  // Dim when user is idle.
	outOfFocusTimeoutSecs: outOfFocusTimeoutSecs // Dim after switching tabs.
});

////// Controls /////
map.addControl(L.control.menubar());
setupToolbar(map);
map.addControl(L.control.scroll());
map.addControl(L.control.alertDialog());
map.addControl(L.control.lokDialog());
map.addControl(L.control.contextMenu());
map.addControl(L.control.infobar());
map.loadDocument(window.socket);

window.socket = map._socket;
window.addEventListener('beforeunload', function () {
	if (map && map._socket) {
		map._socket.close();
	}
});

if (!L.Browser.mobile) {
	L.DomEvent.on(document, 'contextmenu', L.DomEvent.preventDefault);
}
