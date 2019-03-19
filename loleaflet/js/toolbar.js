/* -*- js-indent-level: 8 -*- */
/*
 * LibreOffice Online toolbar
 */

/* global $ closebutton w2ui w2utils vex _ _UNO */
/*eslint indent: [error, "tab", { "outerIIFEBody": 0 }]*/
(function(global) {

var map;

// has to match small screen size requirement
function _inMobileMode() {
	return L.Browser.mobile && $('#main-menu').css('display') === 'none';
}

// mobile device with big screen size
function _inTabletMode() {
	return L.Browser.mobile && !_inMobileMode();
}

function _inDesktopMode() {
	return !L.Browser.mobile;
}

function onDelete(e) {
	if (e !== false) {
		map.deletePage();
	}
}

// make functions visible outside: window.mode.isMobile()
global.mode = {
	isMobile: _inMobileMode,
	isTablet: _inTabletMode,
	isDesktop: _inDesktopMode
};

var nUsers, oneUser, noUser;

function _updateVisibilityForToolbar(toolbar) {
	var isDesktop = _inDesktopMode();
	var isMobile = _inMobileMode();
	var isTablet = _inTabletMode();

	var toShow = [];
	var toHide = [];

	toolbar.items.forEach(function(item) {
		if (((isMobile && item.mobile === false) || (isTablet && item.tablet === false) || (isDesktop && item.desktop === false) || (!window.ThisIsAMobileApp && item.mobilebrowser === false)) && !item.hidden) {
			toHide.push(item.id);
		}
		else if (((isMobile && item.mobile === true) || (isTablet && item.tablet === true) || (isDesktop && item.desktop === true) || (window.ThisIsAMobileApp && item.mobilebrowser === true)) && item.hidden) {
			toShow.push(item.id);
		}
	});

	console.log('explicitly hiding: ' + toHide);
	console.log('explicitly showing: ' + toShow);

	toHide.forEach(function(item) { toolbar.hide(item); });
	toShow.forEach(function(item) { toolbar.show(item); });
}

function _updateToolbarsVisibility() {
	_updateVisibilityForToolbar(w2ui['editbar']);
	_updateVisibilityForToolbar(w2ui['actionbar']);
}

function resizeToolbar() {
	if ($(window).width() !== map.getSize().x) {
		var toolbarUp = w2ui['editbar'];
		var statusbar = w2ui['actionbar'];
		toolbarUp.resize();
		statusbar.resize();
	}
}

function _cancelSearch() {
	var toolbar = w2ui['actionbar'];
	map.resetSelection();
	toolbar.hide('cancelsearch');
	toolbar.disable('searchprev');
	toolbar.disable('searchnext');
	L.DomUtil.get('search-input').value = '';
	map.focus();
}

function onClick(e, id, item, subItem) {
	if (w2ui['editbar'].get(id) !== null) {
		var toolbar = w2ui['editbar'];
		item = toolbar.get(id);
	}
	else if (w2ui.formulabar.get(id) !== null) {
		toolbar = w2ui.formulabar;
		item = toolbar.get(id);
	}
	else if ('document-signing-bar' in w2ui && w2ui['document-signing-bar'].get(id) !== null) {
		toolbar = w2ui['document-signing-bar'];
		item = toolbar.get(id);
	}
	else if (w2ui['actionbar'].get(id) !== null) {
		toolbar = w2ui['actionbar'];
		item = toolbar.get(id);
	}
	else if (w2ui['spreadsheet-toolbar'].get(id) !== null) {
		toolbar = w2ui['spreadsheet-toolbar'];
		item = toolbar.get(id);
	}
	else if (w2ui['presentation-toolbar'].get(id) !== null) {
		toolbar = w2ui['presentation-toolbar'];
		item = toolbar.get(id);
	}
	else {
		throw new Error('unknown id: ' + id);
	}
	var docLayer = map._docLayer;
	if (id !== 'zoomin' && id !== 'zoomout') {
		map.focus();
	}
	if (item.disabled) {
		return;
	}

	if (item.postmessage && item.type === 'button') {
		map.fire('postMessage', {msgId: 'Clicked_Button', args: {Id: item.id} });
	}
	else if (item.uno) {
		if (item.unosheet && map.getDocType() === 'spreadsheet') {
			map.toggleCommandState(item.unosheet);
		}
		else {
			map.toggleCommandState(item.uno);
		}
	}
	else if (id === 'print') {
		map.print();
	}
	else if (id === 'save') {
		map.save(false /* An explicit save should terminate cell edit */, false /* An explicit save should save it again */);
	}
	else if (id === 'repair') {
		map._socket.sendMessage('commandvalues command=.uno:DocumentRepair');
	}
	else if (id === 'zoomin' && map.getZoom() < map.getMaxZoom()) {
		if (map.getDocType() === 'spreadsheet') {
			map.setZoom(14); // 200%
		}
		else {
			map.zoomIn(1);
		}
	}
	else if (id === 'zoomout' && map.getZoom() > map.getMinZoom()) {
		if (map.getDocType() === 'spreadsheet') {
			map.setZoom(10); // 100%
		}
		else {
			map.zoomOut(1);
		}
	}
	else if (item.scale) {
		map.setZoom(item.scale);
	}
	else if (id === 'zoomreset') {
		map.setZoom(map.options.zoom);
	}
	else if (id === 'prev' || id === 'next') {
		if (docLayer._docType === 'text') {
			map.goToPage(id);
		}
		else {
			map.setPart(id);
		}
	}
	else if (id === 'searchprev') {
		map.search(L.DomUtil.get('search-input').value, true);
	}
	else if (id === 'searchnext') {
		map.search(L.DomUtil.get('search-input').value);
	}
	else if (id === 'cancelsearch') {
		_cancelSearch();
	}
	else if (id === 'presentation' && map.getDocType() === 'presentation') {
		map.fire('fullscreen');
	}
	else if (id === 'insertannotation') {
		map.insertComment();
	}
	else if (id === 'insertpage') {
		map.insertPage();
	}
	else if (id === 'duplicatepage') {
		map.duplicatePage();
	}
	else if (id === 'deletepage') {
		vex.dialog.confirm({
			message: _('Are you sure you want to delete this page?'),
			callback: onDelete
		});
	}
	else if (id === 'insertsheet') {
		var nPos = $('#spreadsheet-tab-scroll')[0].childElementCount;
		map.insertPage(nPos + 1);
		$('#spreadsheet-tab-scroll').scrollLeft($('#spreadsheet-tab-scroll').prop('scrollWidth'));
	}
	else if (id === 'firstrecord') {
		$('#spreadsheet-tab-scroll').scrollLeft(0);
	}
	// TODO: We should get visible tab's width instead of 60px
	else if (id === 'nextrecord') {
		$('#spreadsheet-tab-scroll').scrollLeft($('#spreadsheet-tab-scroll').scrollLeft() + 60);
	}
	else if (id === 'prevrecord') {
		$('#spreadsheet-tab-scroll').scrollLeft($('#spreadsheet-tab-scroll').scrollLeft() - 30);
	}
	else if (id === 'lastrecord') {
		$('#spreadsheet-tab-scroll').scrollLeft($('#spreadsheet-tab-scroll').scrollLeft() + 120);
	}
	else if (id === 'insertgraphic' || item.id === 'localgraphic') {
		L.DomUtil.get('insertgraphic').click();
	}
	else if (item.id === 'remotegraphic') {
		map.fire('postMessage', {msgId: 'UI_InsertGraphic'});
	}
	else if (id === 'fontcolor' && typeof e.color !== 'undefined') {
		onColorPick(id, e.color);
	}
	else if (id === 'backcolor' && typeof e.color !== 'undefined') {
		onColorPick(id, e.color)
	}
	else if (id === 'sum') {
		map.sendUnoCommand('.uno:AutoSum');
	}
	else if (id === 'function') {
		L.DomUtil.get('formulaInput').value = '=';
		L.DomUtil.get('formulaInput').focus();
		map.cellEnterString(L.DomUtil.get('formulaInput').value);
	}
	else if (id === 'cancelformula') {
		map.sendUnoCommand('.uno:Cancel');
		w2ui['formulabar'].hide('acceptformula', 'cancelformula');
		w2ui['formulabar'].show('sum', 'function');
	}
	else if (id === 'acceptformula') {
		// focus on map, and press enter
		map.focus();
		map._docLayer._postKeyboardEvent('input',
						 map.keyboard.keyCodes.enter,
						 map.keyboard._toUNOKeyCode(map.keyboard.keyCodes.enter));

		w2ui['formulabar'].hide('acceptformula', 'cancelformula');
		w2ui['formulabar'].show('sum', 'function');
	}
	else if (id.startsWith('StateTableCellMenu') && subItem) {
		e.done(function () {
			var menu = w2ui['actionbar'].get('StateTableCellMenu');
			if (subItem.id === '1') { // 'None' was clicked, remove all other options
				menu.selected = ['1'];
			}
			else { // Something else was clicked, remove the 'None' option from the array
				var index = menu.selected.indexOf('1');
				if (index > -1) {
					menu.selected.splice(index, 1);
				}
			}
			var value = 0;
			for (var it = 0; it < menu.selected.length; it++) {
				value = +value + parseInt(menu.selected[it]);
			}
			var command = {
				'StatusBarFunc': {
					type: 'unsigned short',
					value: value
				}
			};
			map.sendUnoCommand('.uno:StatusBarFunc', command);
		});
	}
	else if (id === 'fold' || id === 'hamburger-tablet') {
		map.toggleMenubar();
	}
	else if (id === 'fullscreen') {
		if (item.checked) {
			toolbar.uncheck(id);
		}
		else {
			toolbar.check(id);
		}
		L.toggleFullScreen();
	}
	else if (id === 'close' || id === 'closemobile') {
		if (window.ThisIsAMobileApp) {
			window.webkit.messageHandlers.lool.postMessage('BYE', '*');
		} else {
			map.fire('postMessage', {msgId: 'close', args: {EverModified: map._everModified, Deprecated: true}});
			map.fire('postMessage', {msgId: 'UI_Close', args: {EverModified: map._everModified}});
		}
		map.remove();
	}
	else {
		map.handleSigningClickEvent(id, item); // this handles a bunch of signing bar click events
	}
}

function setBorders(left, right, bottom, top, horiz, vert) {
	var params = {
		OuterBorder: {
			type : '[]any',
			value : [
				{ type : 'com.sun.star.table.BorderLine2', value : { Color : { type : 'com.sun.star.util.Color', value : 0 }, InnerLineWidth : { type : 'short', value : 0 }, OuterLineWidth : { type : 'short', value : left }, LineDistance : { type : 'short', value : 0 },  LineStyle : { type : 'short', value : 0 }, LineWidth : { type : 'unsigned long', value : 0 } } },
				{ type : 'com.sun.star.table.BorderLine2', value : { Color : { type : 'com.sun.star.util.Color', value : 0 }, InnerLineWidth : { type : 'short', value : 0 }, OuterLineWidth : { type : 'short', value : right }, LineDistance : { type : 'short', value : 0 },  LineStyle : { type : 'short', value : 0 }, LineWidth : { type : 'unsigned long', value : 0 } } },
				{ type : 'com.sun.star.table.BorderLine2', value : { Color : { type : 'com.sun.star.util.Color', value : 0 }, InnerLineWidth : { type : 'short', value : 0 }, OuterLineWidth : { type : 'short', value : bottom }, LineDistance : { type : 'short', value : 0 },  LineStyle : { type : 'short', value : 0 }, LineWidth : { type : 'unsigned long', value : 0 } } },
				{ type : 'com.sun.star.table.BorderLine2', value : { Color : { type : 'com.sun.star.util.Color', value : 0 }, InnerLineWidth : { type : 'short', value : 0 }, OuterLineWidth : { type : 'short', value : top }, LineDistance : { type : 'short', value : 0 },  LineStyle : { type : 'short', value : 0 }, LineWidth : { type : 'unsigned long', value : 0 } } },
				{ type : 'long', value : 0 },
				{ type : 'long', value : 0 },
				{ type : 'long', value : 0 },
				{ type : 'long', value : 0 },
				{ type : 'long', value : 0 }
			]
		},
		InnerBorder: {
			type : '[]any',
			value : [
				{ type : 'com.sun.star.table.BorderLine2', value : { Color : { type : 'com.sun.star.util.Color', value : 0 }, InnerLineWidth : { type : 'short', value : 0 }, OuterLineWidth : { type : 'short', value : horiz }, LineDistance : { type : 'short', value : 0 },  LineStyle : { type : 'short', value : 0 }, LineWidth : { type : 'unsigned long', value : 0 } } },
				{ type : 'com.sun.star.table.BorderLine2', value : { Color : { type : 'com.sun.star.util.Color', value : 0 }, InnerLineWidth : { type : 'short', value : 0 }, OuterLineWidth : { type : 'short', value : vert }, LineDistance : { type : 'short', value : 0 },  LineStyle : { type : 'short', value : 0 }, LineWidth : { type : 'unsigned long', value : 0 } } },
				{ type : 'short', value : 0 },
				{ type : 'short', value : 127 },
				{ type : 'long', value : 0 }
			]
		}};
	map.sendUnoCommand('.uno:SetBorderStyle', params);
}

// close the popup
function closePopup() {
	if ($('#w2ui-overlay-editbar').length > 0) {
		$('#w2ui-overlay-editbar').removeData('keepOpen')[0].hide();
	}
	map.focus();
}

function setBorderStyle(num) {
	switch (num) {
	case 0: map.sendUnoCommand('.uno:FormatCellBorders'); break;

	case 1: setBorders(0, 0, 0, 0, 0, 0); break;
	case 2: setBorders(1, 0, 0, 0, 0, 0); break;
	case 3: setBorders(0, 1, 0, 0, 0, 0); break;
	case 4: setBorders(1, 1, 0, 0, 0, 0); break;

	case 5: setBorders(0, 0, 0, 1, 0, 0); break;
	case 6: setBorders(0, 0, 1, 0, 0, 0); break;
	case 7: setBorders(0, 0, 1, 1, 0, 0); break;
	case 8: setBorders(1, 1, 1, 1, 0, 0); break;

	case 9: setBorders(0, 0, 1, 1, 1, 0); break;
	case 10: setBorders(1, 1, 1, 1, 1, 0); break;
	case 11: setBorders(1, 1, 1, 1, 0, 1); break;
	case 12: setBorders(1, 1, 1, 1, 1, 1); break;

	default: console.log('ignored border: ' + num);
	}

	// TODO we may consider keeping it open in the future if we add border color
	// and style to this popup too
	closePopup();
}

global.setBorderStyle = setBorderStyle;

function setConditionalFormatIconSet(num) {
	var params = {
		IconSet: {
			type : 'short',
			value : num
		}};
	map.sendUnoCommand('.uno:IconSetFormatDialog', params);

	closePopup();
}

global.setConditionalFormatIconSet = setConditionalFormatIconSet;

function insertTable() {
	var rows = 10;
	var cols = 10;
	var $grid = $('.inserttable-grid');
	var $status = $('#inserttable-status');

	// init
	for (var r = 0; r < rows; r++) {
		var $row = $('<div/>').addClass('row');
		$grid.append($row);
		for (var c = 0; c < cols; c++) {
			var $col = $('<div/>').addClass('col');
			$row.append($col);
		}
	}

	// events
	$grid.on({
		mouseover: function () {
			var col = $(this).index() + 1;
			var row = $(this).parent().index() + 1;
			$('.col').removeClass('bright');
			$('.row:nth-child(-n+' + row + ') .col:nth-child(-n+' + col + ')')
			.addClass('bright');
			$status.html(col + 'x' + row);

		},
		click: function() {
			var col = $(this).index() + 1;
			var row = $(this).parent().index() + 1;
			$('.col').removeClass('bright');
			$status.html('<br/>');
			var msg = 'uno .uno:InsertTable {' +
				' "Columns": { "type": "long","value": '
				+ col +
				' }, "Rows": { "type": "long","value": '
				+ row + ' }}';

			map._socket.sendMessage(msg);

			closePopup()
		}
	}, '.col');
}

var shapes = {
	'Basic Shapes': [
		{img: 'basicshapes_rectangle', uno: 'BasicShapes.rectangle'},
		{img: 'basicshapes_round-rectangle', uno: 'BasicShapes.round-rectangle'},
		{img: 'basicshapes_quadrat', uno: 'BasicShapes.quadrat'},
		{img: 'basicshapes_round-quadrat', uno: 'BasicShapes.round-quadrat'},
		{img: 'basicshapes_circle', uno: 'BasicShapes.circle'},
		{img: 'basicshapes_ellipse', uno: 'BasicShapes.ellipse'},

		{img: 'basicshapes_circle-pie', uno: 'BasicShapes.circle-pie'},
		{img: 'basicshapes_isosceles-triangle', uno: 'BasicShapes.isosceles-triangle'},
		{img: 'basicshapes_right-triangle', uno: 'BasicShapes.right-triangle'},
		{img: 'basicshapes_trapezoid', uno: 'BasicShapes.trapezoid'},
		{img: 'basicshapes_diamond', uno: 'BasicShapes.diamond'},
		{img: 'basicshapes_parallelogram', uno: 'BasicShapes.parallelogram'},

		{img: 'basicshapes_pentagon', uno: 'BasicShapes.pentagon'},
		{img: 'basicshapes_hexagon', uno: 'BasicShapes.hexagon'},
		{img: 'basicshapes_octagon', uno: 'BasicShapes.octagon'},
		{img: 'basicshapes_cross', uno: 'BasicShapes.cross'},
		{img: 'basicshapes_ring', uno: 'BasicShapes.ring'},
		{img: 'basicshapes_block-arc', uno: 'BasicShapes.block-arc'},

		{img: 'basicshapes_can', uno: 'BasicShapes.can'},
		{img: 'basicshapes_cube', uno: 'BasicShapes.cube'},
		{img: 'basicshapes_paper', uno: 'BasicShapes.paper'},
		{img: 'basicshapes_frame', uno: 'BasicShapes.frame'}
	],

	'Symbols':  [
		{img: 'symbolshapes', uno: 'SymbolShapes.smiley'},
		{img: 'symbolshapes_sun', uno: 'SymbolShapes.sun'},
		{img: 'symbolshapes_moon', uno: 'SymbolShapes.moon'},
		{img: 'symbolshapes_lightning', uno: 'SymbolShapes.lightning'},
		{img: 'symbolshapes_heart', uno: 'SymbolShapes.heart'},
		{img: 'symbolshapes_flower', uno: 'SymbolShapes.flower'},

		{img: 'symbolshapes_cloud', uno: 'SymbolShapes.cloud'},
		{img: 'symbolshapes_forbidden', uno: 'SymbolShapes.forbidden'},
		{img: 'symbolshapes_puzzle', uno: 'SymbolShapes.puzzle'},
		{img: 'symbolshapes_bracket-pair', uno: 'SymbolShapes.bracket-pair'},
		{img: 'symbolshapes_left-bracket', uno: 'SymbolShapes.left-bracket'},
		{img: 'symbolshapes_right-bracket', uno: 'SymbolShapes.right-bracket'},

		{img: 'symbolshapes_brace-pair', uno: 'SymbolShapes.brace-pair'},
		{img: 'symbolshapes_left-brace', uno: 'SymbolShapes.left-brace'},
		{img: 'symbolshapes_right-brace', uno: 'SymbolShapes.right-brace'},
		{img: 'symbolshapes_quad-bevel', uno: 'SymbolShapes.quad-bevel'},
		{img: 'symbolshapes_octagon-bevel', uno: 'SymbolShapes.octagon-bevel'},
		{img: 'symbolshapes_diamond-bevel', uno: 'SymbolShapes.diamond-bevel'}
	],

	'Block Arrows': [
		{img: 'arrowshapes_left-arrow', uno: 'ArrowShapes.left-arrow'},
		{img: 'arrowshapes_right-arrow', uno: 'ArrowShapes.right-arrow'},
		{img: 'arrowshapes_up-arrow', uno: 'ArrowShapes.up-arrow'},
		{img: 'arrowshapes_down-arrow', uno: 'ArrowShapes.down-arrow'},
		{img: 'arrowshapes_left-right-arrow', uno: 'ArrowShapes.left-right-arrow'},
		{img: 'arrowshapes_up-down-arrow', uno: 'ArrowShapes.up-down-arrow'},

		{img: 'arrowshapes_up-right-arrow', uno: 'ArrowShapes.up-right-arrow'},
		{img: 'arrowshapes_up-right-down-arrow', uno: 'ArrowShapes.up-right-down-arrow'},
		{img: 'arrowshapes_quad-arrow', uno: 'ArrowShapes.quad-arrow'},
		{img: 'arrowshapes_corner-right-arrow', uno: 'ArrowShapes.corner-right-arrow'},
		{img: 'arrowshapes_split-arrow', uno: 'ArrowShapes.split-arrow'},
		{img: 'arrowshapes_striped-right-arrow', uno: 'ArrowShapes.striped-right-arrow'},

		{img: 'arrowshapes_notched-right-arrow', uno: 'ArrowShapes.notched-right-arrow'},
		{img: 'arrowshapes_pentagon-right', uno: 'ArrowShapes.pentagon-right'},
		{img: 'arrowshapes_chevron', uno: 'ArrowShapes.chevron'},
		{img: 'arrowshapes_right-arrow-callout', uno: 'ArrowShapes.right-arrow-callout'},
		{img: 'arrowshapes_left-arrow-callout', uno: 'ArrowShapes.left-arrow-callout'},
		{img: 'arrowshapes_up-arrow-callout', uno: 'ArrowShapes.up-arrow-callout'},

		{img: 'arrowshapes_down-arrow-callout', uno: 'ArrowShapes.down-arrow-callout'},
		{img: 'arrowshapes_left-right-arrow-callout', uno: 'ArrowShapes.left-right-arrow-callout'},
		{img: 'arrowshapes_up-down-arrow-callout', uno: 'ArrowShapes.up-down-arrow-callout'},
		{img: 'arrowshapes_up-right-arrow-callout', uno: 'ArrowShapes.up-right-arrow-callout'},
		{img: 'arrowshapes_quad-arrow-callout', uno: 'ArrowShapes.quad-arrow-callout'},
		{img: 'arrowshapes_circular-arrow', uno: 'ArrowShapes.circular-arrow'},

		{img: 'arrowshapes_split-round-arrow', uno: 'ArrowShapes.split-round-arrow'},
		{img: 'arrowshapes_s-sharped-arrow', uno: 'ArrowShapes.s-sharped-arrow'}
	],

	'Stars': [
		{img: 'starshapes_bang', uno: 'StarShapes.bang'},
		{img: 'starshapes_star4', uno: 'StarShapes.star4'},
		{img: 'starshapes_star5', uno: 'StarShapes.star5'},
		{img: 'starshapes_star6', uno: 'StarShapes.star6'},
		{img: 'starshapes_star8', uno: 'StarShapes.star8'},
		{img: 'starshapes_star12', uno: 'StarShapes.star12'},

		{img: 'starshapes_star24', uno: 'StarShapes.star24'},
		{img: 'starshapes_concave-star6', uno: 'StarShapes.concave-star6'},
		{img: 'starshapes_vertical-scroll', uno: 'StarShapes.vertical-scroll'},
		{img: 'starshapes_horizontal-scroll', uno: 'StarShapes.horizontal-scroll'},
		{img: 'starshapes_signet', uno: 'StarShapes.signet'},
		{img: 'starshapes_doorplate', uno: 'StarShapes.doorplate'}
	],

	'Callouts': [
		{img: 'calloutshapes_rectangular-callout', uno: 'CalloutShapes.rectangular-callout'},
		{img: 'calloutshapes_round-rectangular-callout', uno: 'CalloutShapes.round-rectangular-callout'},
		{img: 'calloutshapes_round-callout', uno: 'CalloutShapes.round-callout'},
		{img: 'calloutshapes_cloud-callout', uno: 'CalloutShapes.cloud-callout'},
		{img: 'calloutshapes_line-callout-1', uno: 'CalloutShapes.line-callout-1'},
		{img: 'calloutshapes_line-callout-2', uno: 'CalloutShapes.line-callout-2'},
		{img: 'calloutshapes_line-callout-3', uno: 'CalloutShapes.line-callout-3'}
	],

	'Flowchart': [
		{img: 'flowchartshapes_flowchart-process', uno: 'FlowchartShapes.flowchart-process'},
		{img: 'flowchartshapes_flowchart-alternate-process', uno: 'FlowchartShapes.flowchart-alternate-process'},
		{img: 'flowchartshapes_flowchart-decision', uno: 'FlowchartShapes.flowchart-decision'},
		{img: 'flowchartshapes_flowchart-data', uno: 'FlowchartShapes.flowchart-data'},
		{img: 'flowchartshapes_flowchart-predefined-process', uno: 'FlowchartShapes.flowchart-predefined-process'},
		{img: 'flowchartshapes_flowchart-internal-storage', uno: 'FlowchartShapes.flowchart-internal-storage'},

		{img: 'flowchartshapes_flowchart-document', uno: 'FlowchartShapes.flowchart-document'},
		{img: 'flowchartshapes_flowchart-multidocument', uno: 'FlowchartShapes.flowchart-multidocument'},
		{img: 'flowchartshapes_flowchart-terminator', uno: 'FlowchartShapes.flowchart-terminator'},
		{img: 'flowchartshapes_flowchart-preparation', uno: 'FlowchartShapes.flowchart-preparation'},
		{img: 'flowchartshapes_flowchart-manual-input', uno: 'FlowchartShapes.flowchart-manual-input'},
		{img: 'flowchartshapes_flowchart-manual-operation', uno: 'FlowchartShapes.flowchart-manual-operation'},

		{img: 'flowchartshapes_flowchart-connector', uno: 'FlowchartShapes.flowchart-connector'},
		{img: 'flowchartshapes_flowchart-off-page-connector', uno: 'FlowchartShapes.flowchart-off-page-connector'},
		{img: 'flowchartshapes_flowchart-card', uno: 'FlowchartShapes.flowchart-card'},
		{img: 'flowchartshapes_flowchart-punched-tape', uno: 'FlowchartShapes.flowchart-punched-tape'},
		{img: 'flowchartshapes_flowchart-summing-junction', uno: 'FlowchartShapes.flowchart-summing-junction'},
		{img: 'flowchartshapes_flowchart-or', uno: 'FlowchartShapes.flowchart-or'},

		{img: 'flowchartshapes_flowchart-collate', uno: 'FlowchartShapes.flowchart-collate'},
		{img: 'flowchartshapes_flowchart-sort', uno: 'FlowchartShapes.flowchart-sort'},
		{img: 'flowchartshapes_flowchart-extract', uno: 'FlowchartShapes.flowchart-extract'},
		{img: 'flowchartshapes_flowchart-merge', uno: 'FlowchartShapes.flowchart-merge'},
		{img: 'flowchartshapes_flowchart-stored-data', uno: 'FlowchartShapes.flowchart-stored-data'},
		{img: 'flowchartshapes_flowchart-delay', uno: 'FlowchartShapes.flowchart-delay'},

		{img: 'flowchartshapes_flowchart-sequential-access', uno: 'FlowchartShapes.flowchart-sequential-access'},
		{img: 'flowchartshapes_flowchart-magnetic-disk', uno: 'FlowchartShapes.flowchart-magnetic-disk'},
		{img: 'flowchartshapes_flowchart-direct-access-storage', uno: 'FlowchartShapes.flowchart-direct-access-storage'},
		{img: 'flowchartshapes_flowchart-display', uno: 'FlowchartShapes.flowchart-display'}
	]
};

function insertShapes() {
	var width = 10;
	var $grid = $('.insertshape-grid');

	if ($grid.children().size() > 0)
		return;

	for (var s in shapes) {
		var $rowHeader = $('<div/>').addClass('row-header loleaflet-font').append(_(s));
		$grid.append($rowHeader);

		var rows = Math.ceil(shapes[s].length / width);
		var idx = 0;
		for (var r = 0; r < rows; r++) {
			var $row = $('<div/>').addClass('row');
			$grid.append($row);
			for (var c = 0; c < width; c++) {
				if (idx >= shapes[s].length) {
					break;
				}
				var shape = shapes[s][idx++];
				var $col = $('<div/>').addClass('col w2ui-icon').addClass(shape.img);
				$col.data('uno', shape.uno);
				$row.append($col);
			}

			if (idx >= shapes[s].length)
				break;
		}
	}

	$grid.on({
		click: function(e) {
			map.sendUnoCommand('.uno:' + $(e.target).data().uno);
			closePopup();
		}
	});
}

function onColorPick(id, color) {
	if (map.getPermission() !== 'edit') {
		return;
	}
    // no fill or automatic color is -1
	if (color === '') {
		color = -1;
	}
	// transform from #FFFFFF to an Int
	else {
		color = parseInt(color.replace('#', ''), 16);
	}
	var command = {};
	var fontcolor, backcolor;
	if (id === 'fontcolor') {
		fontcolor = {'text': 'FontColor',
					 'spreadsheet': 'Color',
					 'presentation': 'Color'}[map.getDocType()];
		command[fontcolor] = {};
		command[fontcolor].type = 'long';
		command[fontcolor].value = color;
		var uno = '.uno:' + fontcolor;
	}
	else if (id === 'backcolor') {
		backcolor = {'text': 'BackColor',
					 'spreadsheet': 'BackgroundColor',
					 'presentation': 'CharBackColor'}[map.getDocType()];
		command[backcolor] = {};
		command[backcolor].type = 'long';
		command[backcolor].value = color;
		uno = '.uno:' + backcolor;
	}
	map.sendUnoCommand(uno, command);
	map.focus();
}

function hideTooltip(toolbar, id) {
	if (toolbar.touchStarted) {
		setTimeout(function() {
			toolbar.tooltipHide(id, {});
		}, 5000);
		toolbar.touchStarted = false;
	}
}

var stylesSelectValue;
var fontsSelectValue;
var fontsizesSelectValue;

// mobile:false means hide it both for normal Online used from a mobile browser, and in a mobile app
// mobilebrowser:false means hide it for normal Online used from a mobile browser, but don't hide it in a mobile app

function createToolbar() {
	var toolItems = [
		{type: 'button',  id: 'closemobile',  img: 'closemobile', desktop: false, mobile: false, tablet: true, hidden: true},
		{type: 'button',  id: 'save', img: 'save', hint: _UNO('.uno:Save')},
		{type: 'button',  id: 'print', img: 'print', hint: _UNO('.uno:Print', 'text'), mobile: false, tablet: false},
		{type: 'break', id: 'savebreak', mobile: false},
		{type: 'button',  id: 'undo',  img: 'undo', hint: _UNO('.uno:Undo'), uno: 'Undo', disabled: true, mobile: false},
		{type: 'button',  id: 'redo',  img: 'redo', hint: _UNO('.uno:Redo'), uno: 'Redo', disabled: true, mobile: false},
		{type: 'button',  id: 'formatpaintbrush',  img: 'copyformat', hint: _UNO('.uno:FormatPaintbrush'), uno: 'FormatPaintbrush', mobile: false},
		{type: 'button',  id: 'reset',  img: 'deleteformat', hint: _UNO('.uno:ResetAttributes', 'text'), uno: 'ResetAttributes', mobile: false},
		{type: 'break', mobile: false},
		{type: 'menu-radio', id: 'zoom', text: '100%',
			selected: 'zoom100',
			mobile: false, tablet: false,
			items: [
				{ id: 'zoom50', text: '50%', scale: 6},
				{ id: 'zoom60', text: '60%', scale: 7},
				{ id: 'zoom70', text: '70%', scale: 8},
				{ id: 'zoom85', text: '85%', scale: 9},
				{ id: 'zoom100', text: '100%', scale: 10},
				{ id: 'zoom120', text: '120%', scale: 11},
				{ id: 'zoom150', text: '150%', scale: 12},
				{ id: 'zoom175', text: '175%', scale: 13},
				{ id: 'zoom200', text: '200%', scale: 14}
			]
		},
		{type: 'break', mobile: false, tablet: false,},
		{type: 'html', id: 'styles',
			html: '<select class="styles-select"><option>Default Style</option></select>',
			onRefresh: function (edata) {
				if (!edata.item.html) {
					edata.isCancelled = true;
				} else {
					$.extend(edata, { onComplete: function (e) {
						$('.styles-select').select2();
						e.item.html = undefined;
					}});
				}
			}, hidden: true, desktop: true, mobile: false, tablet: false},
		{type: 'html', id: 'fonts',
			html: '<select class="fonts-select"><option>Liberation Sans</option></select>',
			onRefresh: function (edata) {
				if (!edata.item.html) {
					edata.isCancelled = true;
				} else {
					$.extend(edata, { onComplete: function (e) {
						$('.fonts-select').select2();
						e.item.html = undefined;
					}});
				}
			}, mobile: false},
		{type: 'html',   id: 'fontsizes',
			html: '<select class="fontsizes-select"><option>14</option></select>',
			onRefresh: function (edata) {
				if (!edata.item.html) {
					edata.isCancelled = true;
				} else {
					$.extend(edata, { onComplete: function (e) {
						$('.fontsizes-select').select2({ dropdownAutoWidth: true, width: 'auto'});
						e.item.html = undefined;
					}});
				}
			}, mobile: false},
		{type: 'break', mobile: false, tablet: false },
		{type: 'button',  id: 'bold',  img: 'bold', hint: _UNO('.uno:Bold'), uno: 'Bold'},
		{type: 'button',  id: 'italic', img: 'italic', hint: _UNO('.uno:Italic'), uno: 'Italic'},
		{type: 'button',  id: 'underline',  img: 'underline', hint: _UNO('.uno:Underline'), uno: 'Underline'},
		{type: 'button',  id: 'strikeout', img: 'strikeout', hint: _UNO('.uno:Strikeout'), uno: 'Strikeout'},
		{type: 'break'},
		{type: 'text-color',  id: 'fontcolor', img: 'textcolor', hint: _UNO('.uno:FontColor')},
		{type: 'color',  id: 'backcolor', img: 'backcolor', hint: _UNO('.uno:BackgroundColor')},
		{type: 'break' , mobile:false},
		{type: 'button',  id: 'leftpara',  img: 'alignleft', hint: _UNO('.uno:LeftPara', '', true), uno: 'LeftPara', hidden: true, unosheet: 'AlignLeft', disabled: true},
		{type: 'button',  id: 'centerpara',  img: 'alignhorizontal', hint: _UNO('.uno:CenterPara', '', true), uno: 'CenterPara', hidden: true, unosheet: 'AlignHorizontalCenter', disabled: true},
		{type: 'button',  id: 'rightpara',  img: 'alignright', hint: _UNO('.uno:RightPara', '', true), uno: 'RightPara', hidden: true, unosheet: 'AlignRight', disabled: true},
		{type: 'button',  id: 'justifypara',  img: 'alignblock', hint: _UNO('.uno:JustifyPara', '', true), uno: 'JustifyPara', hidden: true, unosheet: '', disabled: true},
		{type: 'break', id: 'breakpara', hidden: true},
		{type: 'drop',  id: 'setborderstyle',  img: 'setborderstyle', hint: _('Borders'), hidden: true,
			html: '<table id="setborderstyle-grid"><tr><td class="w2ui-tb-image w2ui-icon frame01" onclick="setBorderStyle(1)"></td>' +
			      '<td class="w2ui-tb-image w2ui-icon frame02" onclick="setBorderStyle(2)"></td><td class="w2ui-tb-image w2ui-icon frame03" onclick="setBorderStyle(3)"></td>' +
			      '<td class="w2ui-tb-image w2ui-icon frame04" onclick="setBorderStyle(4)"></td></tr><tr><td class="w2ui-tb-image w2ui-icon frame05" onclick="setBorderStyle(5)"></td>' +
			      '<td class="w2ui-tb-image w2ui-icon frame06" onclick="setBorderStyle(6)"></td><td class="w2ui-tb-image w2ui-icon frame07" onclick="setBorderStyle(7)"></td>' +
			      '<td class="w2ui-tb-image w2ui-icon frame08" onclick="setBorderStyle(8)"></td></tr><tr><td class="w2ui-tb-image w2ui-icon frame09" onclick="setBorderStyle(9)"></td>' +
			      '<td class="w2ui-tb-image w2ui-icon frame10" onclick="setBorderStyle(10)"></td><td class="w2ui-tb-image w2ui-icon frame11" onclick="setBorderStyle(11)"></td>' +
			      '<td class="w2ui-tb-image w2ui-icon frame12" onclick="setBorderStyle(12)"></td></tr><tr>' +
			      '<td colspan="4" class="w2ui-tb-image w2ui-icon frame13" onclick="setBorderStyle(0)"><div id="div-frame13">' + _('More...') + '</div></td></tr></table>'
		},
		{type: 'button',  id: 'togglemergecells',  img: 'togglemergecells', hint: _UNO('.uno:ToggleMergeCells', 'spreadsheet', true), hidden: true, uno: 'ToggleMergeCells', disabled: true},
		{type: 'break', id: 'breakmergecells', hidden: true},
		{type: 'menu', id: 'textalign', img: 'alignblock', hint: _UNO('.uno:TextAlign'), hidden: true,
			items: [
				{id: 'alignleft', text: _UNO('.uno:AlignLeft', 'spreadsheet', true), icon: 'alignleft', uno: 'AlignLeft'},
				{id: 'alignhorizontalcenter', text: _UNO('.uno:AlignHorizontalCenter', 'spreadsheet', true), icon: 'alignhorizontal', uno: 'AlignHorizontalCenter'},
				{id: 'alignright', text: _UNO('.uno:AlignRight', 'spreadsheet', true), icon: 'alignright', uno: 'AlignRight'},
				{id: 'alignblock', text: _UNO('.uno:AlignBlock', 'spreadsheet', true), icon: 'alignblock', uno: 'AlignBlock'},
			]},
		{type: 'menu',  id: 'linespacing',  img: 'linespacing', hint: _UNO('.uno:FormatSpacingMenu'), hidden: true,
			items: [
				{id: 'spacepara1', text: _UNO('.uno:SpacePara1'), uno: 'SpacePara1'},
				{id: 'spacepara15', text: _UNO('.uno:SpacePara15'), uno: 'SpacePara15'},
				{id: 'spacepara2', text: _UNO('.uno:SpacePara2'), uno: 'SpacePara2'},
				{type: 'break'},
				{id: 'paraspaceincrease', text: _UNO('.uno:ParaspaceIncrease'), uno: 'ParaspaceIncrease'},
				{id: 'paraspacedecrease', text: _UNO('.uno:ParaspaceDecrease'), uno: 'ParaspaceDecrease'}
			]},
		{type: 'button',  id: 'wraptext',  img: 'wraptext', hint: _UNO('.uno:WrapText', 'spreadsheet', true), hidden: true, uno: 'WrapText', disabled: true},
		{type: 'break', id: 'breakspacing', hidden: true},
		{type: 'button',  id: 'defaultnumbering',  img: 'numbering', hint: _UNO('.uno:DefaultNumbering', '', true), hidden: true, uno: 'DefaultNumbering', disabled: true},
		{type: 'button',  id: 'defaultbullet',  img: 'bullet', hint: _UNO('.uno:DefaultBullet', '', true), hidden: true, uno: 'DefaultBullet', disabled: true},
		{type: 'break', id: 'breakbullet', hidden: true},
		{type: 'button',  id: 'incrementindent',  img: 'incrementindent', hint: _UNO('.uno:IncrementIndent', '', true), uno: 'IncrementIndent', hidden: true, disabled: true},
		{type: 'button',  id: 'decrementindent',  img: 'decrementindent', hint: _UNO('.uno:DecrementIndent', '', true), uno: 'DecrementIndent', hidden: true, disabled: true},
		{type: 'break', id: 'breakindent', hidden: true},
		{type: 'button',  id: 'sortascending',  img: 'sortascending', hint: _UNO('.uno:SortAscending', 'spreadsheet', true), uno: 'SortAscending', disabled: true, hidden: true},
		{type: 'button',  id: 'sortdescending',  img: 'sortdescending', hint: _UNO('.uno:SortDescending', 'spreadsheet', true), uno: 'SortDescending', disabled: true, hidden: true},
		{type: 'break', id: 'breaksorting', hidden: true},
		{type: 'drop', id: 'conditionalformaticonset',  img: 'conditionalformatdialog', hint: _UNO('.uno:ConditionalFormatMenu', 'spreadsheet', true), hidden: true,
			html: '<table id="conditionalformatmenu-grid"><tr>' +
				  '<td class="w2ui-tb-image w2ui-icon iconset00" onclick="setConditionalFormatIconSet(0)"/><td class="w2ui-tb-image w2ui-icon iconset01" onclick="setConditionalFormatIconSet(1)"/><td class="w2ui-tb-image w2ui-icon iconset02" onclick="setConditionalFormatIconSet(2)"/></tr><tr>' +
				  '<td class="w2ui-tb-image w2ui-icon iconset03" onclick="setConditionalFormatIconSet(3)"/><td class="w2ui-tb-image w2ui-icon iconset04" onclick="setConditionalFormatIconSet(4)"/><td class="w2ui-tb-image w2ui-icon iconset05" onclick="setConditionalFormatIconSet(5)"/></tr><tr>' +
				  '<td class="w2ui-tb-image w2ui-icon iconset06" onclick="setConditionalFormatIconSet(6)"/><td class="w2ui-tb-image w2ui-icon iconset08" onclick="setConditionalFormatIconSet(8)"/><td class="w2ui-tb-image w2ui-icon iconset09" onclick="setConditionalFormatIconSet(9)"/></tr><tr>' + // iconset07 deliberately left out, see the .css for the reason
				  '<td class="w2ui-tb-image w2ui-icon iconset10" onclick="setConditionalFormatIconSet(10)"/><td class="w2ui-tb-image w2ui-icon iconset11" onclick="setConditionalFormatIconSet(11)"/><td class="w2ui-tb-image w2ui-icon iconset12" onclick="setConditionalFormatIconSet(12)"/></tr><tr>' +
				  '<td class="w2ui-tb-image w2ui-icon iconset13" onclick="setConditionalFormatIconSet(13)"/><td class="w2ui-tb-image w2ui-icon iconset14" onclick="setConditionalFormatIconSet(14)"/><td class="w2ui-tb-image w2ui-icon iconset15" onclick="setConditionalFormatIconSet(15)"/></tr><tr>' +
				  '<td class="w2ui-tb-image w2ui-icon iconset16" onclick="setConditionalFormatIconSet(16)"/><td class="w2ui-tb-image w2ui-icon iconset17" onclick="setConditionalFormatIconSet(17)"/><td class="w2ui-tb-image w2ui-icon iconset18" onclick="setConditionalFormatIconSet(18)"/></tr><tr>' +
				  '<td class="w2ui-tb-image w2ui-icon iconset19" onclick="setConditionalFormatIconSet(19)"/><td class="w2ui-tb-image w2ui-icon iconset20" onclick="setConditionalFormatIconSet(20)"/><td class="w2ui-tb-image w2ui-icon iconset21" onclick="setConditionalFormatIconSet(21)"/></tr></table>'
		},
		{type: 'button',  id: 'numberformatcurrency',  img: 'numberformatcurrency', hint: _UNO('.uno:NumberFormatCurrency', 'spreadsheet', true), hidden: true, uno: 'NumberFormatCurrency', disabled: true},
		{type: 'button',  id: 'numberformatpercent',  img: 'numberformatpercent', hint: _UNO('.uno:NumberFormatPercent', 'spreadsheet', true), hidden: true, uno: 'NumberFormatPercent', disabled: true},
		{type: 'button',  id: 'numberformatdecdecimals',  img: 'numberformatdecdecimals', hint: _UNO('.uno:NumberFormatDecDecimals', 'spreadsheet', true), hidden: true, uno: 'NumberFormatDecDecimals', disabled: true},
		{type: 'button',  id: 'numberformatincdecimals',  img: 'numberformatincdecimals', hint: _UNO('.uno:NumberFormatIncDecimals', 'spreadsheet', true), hidden: true, uno: 'NumberFormatIncDecimals', disabled: true},
		{type: 'break',   id: 'break-number', hidden: true},
		{type: 'button',  id: 'insertannotation', img: 'annotation', hint: _UNO('.uno:InsertAnnotation', '', true), hidden: true},
		{type: 'drop',  id: 'inserttable',  img: 'inserttable', hint: _('Insert table'), hidden: true, overlay: {onShow: insertTable},
		 html: '<div id="inserttable-wrapper"><div id="inserttable-popup" class="inserttable-pop ui-widget ui-corner-all"><div class="inserttable-grid"></div><div id="inserttable-status" class="loleaflet-font" style="padding: 5px;"><br/></div></div></div>'},
		{type: 'button',  id: 'insertgraphic',  img: 'insertgraphic', hint: _UNO('.uno:InsertGraphic', '', true)},
		{type: 'menu', id: 'menugraphic', img: 'insertgraphic', hint: _UNO('.uno:InsertGraphic', '', true), hidden: true,
			items: [
				{id: 'localgraphic', text: _('Insert Local Image')},
				{id: 'remotegraphic', text: _UNO('.uno:InsertGraphic', '', true)},
			]},
		{type: 'button',  id: 'insertobjectchart',  img: 'insertobjectchart', hint: _UNO('.uno:InsertObjectChart', '', true), uno: 'InsertObjectChart'},
		{type: 'drop',  id: 'insertshapes',  img: 'basicshapes_ellipse', hint: _('Insert shapes'), overlay: {onShow: insertShapes},
			html: '<div id="insertshape-wrapper"><div id="insertshape-popup" class="insertshape-pop ui-widget ui-corner-all"><div class="insertshape-grid"></div></div></div>'},

		{type: 'button',  id: 'link',  img: 'link', hint: _UNO('.uno:HyperlinkDialog'), uno: 'HyperlinkDialog', disabled: true},
		{type: 'button',  id: 'insertsymbol', img: 'insertsymbol', hint: _UNO('.uno:InsertSymbol', '', true), uno: 'InsertSymbol'},
		{type: 'spacer'},
		{type: 'button',  id: 'edit',  img: 'edit'},
		{type: 'button',  id: 'fold',  img: 'fold', desktop: true, mobile: false, hidden: true},
		{type: 'button',  id: 'hamburger-tablet',  img: 'hamburger', desktop: false, mobile: false, tablet: true, hidden: true}
	];

	if (_inMobileMode()) {
		$('#mobile-edit-button').show();
		initMobileToolbar(toolItems);
	} else {
		$('#toolbar-down').show();
		initNormalToolbar(toolItems);
	}
}

function initMobileToolbar(toolItems) {
	var toolbar = $('#toolbar-up');
	toolbar.w2toolbar({
		name: 'actionbar',
		tooltip: 'bottom',
		items: [
			{type: 'button',  id: 'closemobile',  img: 'closemobile'},
			{type: 'spacer'},
			{type: 'button',  id: 'prev', img: 'prev', hint: _UNO('.uno:PageUp', 'text'), hidden: true},
			{type: 'button',  id: 'next', img: 'next', hint: _UNO('.uno:PageDown', 'text'), hidden: true},
			{type: 'button',  id: 'undo',  img: 'undo', hint: _UNO('.uno:Undo'), uno: 'Undo', disabled: true},
			{type: 'button',  id: 'redo',  img: 'redo', hint: _UNO('.uno:Redo'), uno: 'Redo', disabled: true},
			{type: 'button',  id: 'fullscreen', img: 'fullscreen', hint: _UNO('.uno:FullScreen', 'text')},
			{type: 'drop', id: 'userlist', img: 'users', hidden: true, html: '<div id="userlist_container"><table id="userlist_table"><tbody></tbody></table>' +
				'<hr><table class="loleaflet-font" id="editor-btn">' +
				'<tr>' +
				'<td><input type="checkbox" name="alwaysFollow" id="follow-checkbox" onclick="editorUpdate(event)"></td>' +
				'<td>' + _('Always follow the editor') + '</td>' +
				'</tr>' +
				'</table>' +
				'<p id="currently-msg">' + _('Current') + ' - <b><span id="current-editor"></span></b></p>' +
				'</div>'
			},
		],
		onClick: function (e) {
			onClick(e, e.target);
			hideTooltip(this, e.target);
		},
		onRefresh: function() {
			var showUserList = map['wopi'].HideUserList !== null &&
								map['wopi'].HideUserList !== undefined &&
								$.inArray('true', map['wopi'].HideUserList) < 0 &&
								((window.mode.isMobile() && $.inArray('mobile', map['wopi'].HideUserList) < 0) ||
								(window.mode.isTablet() && $.inArray('tablet', map['wopi'].HideUserList) < 0));
			if (this.get('userlist').hidden == true && showUserList) {
				this.show('userlist');
				this.show('userlistbreak');
				map.on('deselectuser', deselectUser);
				map.on('addview', onAddView);
				map.on('removeview', onRemoveView);
			}
		}
	});
	toolbar.bind('touchstart', function(e) {
		w2ui['actionbar'].touchStarted = true;
		var touchEvent = e.originalEvent;
		if (touchEvent && touchEvent.touches.length > 1) {
			L.DomEvent.preventDefault(e);
		}
	});

	toolbar = $('#formulabar');
	toolbar.w2toolbar({
		name: 'formulabar',
		tooltip: 'bottom',
		hidden: true,
		items: [
			{type: 'html',  id: 'left'},
			{type: 'html', id: 'address', html: '<input id="addressInput" type="text">'},
			{type: 'break'},
			{type: 'button',  id: 'sum',  img: 'autosum', hint: _('Sum')},
			{type: 'button',  id: 'function',  img: 'equal', hint: _('Function')},
			{type: 'button', hidden: true, id: 'cancelformula',  img: 'cancel', hint: _('Cancel')},
			{type: 'button', hidden: true, id: 'acceptformula',  img: 'accepttrackedchanges', hint: _('Accept')},
			{type: 'html', id: 'formula', html: '<input id="formulaInput" type="text">'}
		],
		onClick: function (e) {
			onClick(e, e.target);
			hideTooltip(this, e.target);
		},
		onRefresh: function() {
			$('#addressInput').off('keyup', onAddressInput).on('keyup', onAddressInput);
			$('#formulaInput').off('keyup', onFormulaInput).on('keyup', onFormulaInput);
			$('#formulaInput').off('blur', onFormulaBarBlur).on('blur', onFormulaBarBlur);
			$('#formulaInput').off('focus', onFormulaBarFocus).on('focus', onFormulaBarFocus);
		}
	});
	toolbar.bind('touchstart', function(e) {
		w2ui['formulabar'].touchStarted = true;
		var touchEvent = e.originalEvent;
		if (touchEvent && touchEvent.touches.length > 1) {
			L.DomEvent.preventDefault(e);
		}
	});

	$(w2ui.formulabar.box).find('.w2ui-scroll-left, .w2ui-scroll-right').hide();
	w2ui.formulabar.on('resize', function(target, e) {
		e.isCancelled = true;
	});

	toolbar = $('#spreadsheet-toolbar');
	toolbar.w2toolbar({
		name: 'spreadsheet-toolbar',
		tooltip: 'bottom',
		hidden: true,
		items: [
			{type: 'button',  id: 'firstrecord',  img: 'firstrecord', hint: _('First sheet')},
			{type: 'button',  id: 'prevrecord',  img: 'prevrecord', hint: _('Previous sheet')},
			{type: 'button',  id: 'nextrecord',  img: 'nextrecord', hint: _('Next sheet')},
			{type: 'button',  id: 'lastrecord',  img: 'lastrecord', hint: _('Last sheet')},
			{type: 'button',  id: 'insertsheet', img: 'insertsheet', hint: _('Insert sheet')}
		],
		onClick: function (e) {
			onClick(e, e.target);
			hideTooltip(this, e.target);
		}
	});
	toolbar.bind('touchstart', function(e) {
		w2ui['spreadsheet-toolbar'].touchStarted = true;
		var touchEvent = e.originalEvent;
		if (touchEvent && touchEvent.touches.length > 1) {
			L.DomEvent.preventDefault(e);
		}
	});

	toolbar = $('#presentation-toolbar');
	toolbar.w2toolbar({
		name: 'presentation-toolbar',
		tooltip: 'bottom',
		hidden: true,
		items: []
	});

	toolbar = $('#toolbar-down');
	toolbar.w2toolbar({
		name: 'editbar',
		tooltip: 'top',
		items: toolItems,
		onClick: function (e) {
			onClick(e, e.target);
			hideTooltip(this, e.target);
		},
		onRefresh: function(edata) {
			if (edata.target === 'styles' || edata.target === 'fonts' || edata.target === 'fontsizes') {
				var toolItem = $(this.box).find('#tb_'+ this.name +'_item_'+ w2utils.escapeId(edata.item.id));
				if (edata.item.hidden) {
					toolItem.css('display', 'none');
				} else {
					toolItem.css('display', '');
				}
				updateCommandValues(edata.target);
			}

			if (edata.target === 'editbar' && map.getDocType() === 'presentation') {
				// Fill the style select box if not yet filled
				if ($('.styles-select')[0] && $('.styles-select')[0].length === 1) {
					var data = [''];
					// Inserts a separator element
					data = data.concat({text: '\u2500\u2500\u2500\u2500\u2500\u2500', disabled: true});

					L.Styles.impressLayout.forEach(function(layout) {
						data = data.concat({id: layout.id, text: _(layout.text)});
					}, this);

					$('.styles-select').select2({
						data: data,
						placeholder: _UNO('.uno:LayoutStatus', 'presentation')
					});
					$('.styles-select').on('select2:select', onStyleSelect);
				}
			}

			if (edata.target === 'inserttable')
				insertTable();

			if (edata.target === 'insertshapes')
				insertShapes();
		}
	});

	toolbar.bind('touchstart', function(e) {
		w2ui['editbar'].touchStarted = true;
		var touchEvent = e.originalEvent;
		if (touchEvent && touchEvent.touches.length > 1) {
			L.DomEvent.preventDefault(e);
		}
	});
}

function initNormalToolbar(toolItems) {
	var toolbar = $('#toolbar-up');
	toolbar.w2toolbar({
		name: 'editbar',
		tooltip: 'bottom',
		items: toolItems,
		onClick: function (e) {
			onClick(e, e.target);
			hideTooltip(this, e.target);
		},
		onRefresh: function(event) {
			if (event.target === 'editbar' && map.getDocType() === 'presentation') {
				// Fill the style select box if not yet filled
				if ($('.styles-select')[0] && $('.styles-select')[0].length === 1) {
					var data = [''];
					// Inserts a separator element
					data = data.concat({text: '\u2500\u2500\u2500\u2500\u2500\u2500', disabled: true});

					L.Styles.impressLayout.forEach(function(layout) {
						data = data.concat({id: layout.id, text: _(layout.text)});
					}, this);

					$('.styles-select').select2({
						data: data,
						placeholder: _UNO('.uno:LayoutStatus', 'presentation')
					});
					$('.styles-select').on('select2:select', onStyleSelect);
				}
			}

			if ((event.target === 'styles' || event.target === 'fonts' || event.target === 'fontsizes') && event.item) {
				var toolItem = $(this.box).find('#tb_'+ this.name +'_item_'+ w2utils.escapeId(event.item.id));
				if ((_inDesktopMode() && event.item.desktop == false)
					|| (_inTabletMode() && event.item.tablet == false)) {
					toolItem.css('display', 'none');
				} else {
					toolItem.css('display', '');
				}

				updateCommandValues(event.target);
			}

			if (event.target === 'inserttable')
				insertTable();

			if (event.target === 'insertshapes')
				insertShapes();
		}
	});

	toolbar.bind('touchstart', function() {
		w2ui['editbar'].touchStarted = true;
	});

	toolbar = $('#formulabar');
	toolbar.w2toolbar({
		name: 'formulabar',
		tooltip: 'bottom',
		hidden: true,
		items: [
			{type: 'html',  id: 'left'},
			{type: 'html', id: 'address', html: '<input id="addressInput" type="text">'},
			{type: 'break'},
			{type: 'button',  id: 'sum',  img: 'autosum', hint: _('Sum')},
			{type: 'button',  id: 'function',  img: 'equal', hint: _('Function')},
			{type: 'button', hidden: true, id: 'cancelformula',  img: 'cancel', hint: _('Cancel')},
			{type: 'button', hidden: true, id: 'acceptformula',  img: 'accepttrackedchanges', hint: _('Accept')},
			{type: 'html', id: 'formula', html: '<input id="formulaInput" type="text">'}
		],
		onClick: function (e) {
			onClick(e, e.target);
			hideTooltip(this, e.target);
		},
		onRefresh: function() {
			$('#addressInput').off('keyup', onAddressInput).on('keyup', onAddressInput);
			$('#formulaInput').off('keyup', onFormulaInput).on('keyup', onFormulaInput);
			$('#formulaInput').off('blur', onFormulaBarBlur).on('blur', onFormulaBarBlur);
			$('#formulaInput').off('focus', onFormulaBarFocus).on('focus', onFormulaBarFocus);
		}
	});
	toolbar.bind('touchstart', function() {
		w2ui['formulabar'].touchStarted = true;
	});

	$(w2ui.formulabar.box).find('.w2ui-scroll-left, .w2ui-scroll-right').hide();
	w2ui.formulabar.on('resize', function(target, e) {
		e.isCancelled = true;
	});

	if (L.DomUtil.get('document-signing-bar') !== null) {
		toolbar = $('#document-signing-bar');
		toolbar.w2toolbar({
			name: 'document-signing-bar',
			tooltip: 'bottom',
			items: map.setupSigningToolbarItems(),
			onClick: function (e) {
				onClick(e, e.target);
				hideTooltip(this, e.target);
			},
			onRefresh: function() {
			}
		});
		toolbar.bind('touchstart', function() {
			w2ui['document-signing-bar'].touchStarted = true;
		});
	}

	toolbar = $('#spreadsheet-toolbar')
	toolbar.w2toolbar({
		name: 'spreadsheet-toolbar',
		tooltip: 'bottom',
		hidden: true,
		items: [
			{type: 'button',  id: 'firstrecord',  img: 'firstrecord', hint: _('First sheet')},
			{type: 'button',  id: 'prevrecord',  img: 'prevrecord', hint: _('Previous sheet')},
			{type: 'button',  id: 'nextrecord',  img: 'nextrecord', hint: _('Next sheet')},
			{type: 'button',  id: 'lastrecord',  img: 'lastrecord', hint: _('Last sheet')},
			{type: 'button',  id: 'insertsheet', img: 'insertsheet', hint: _('Insert sheet')}
		],
		onClick: function (e) {
			onClick(e, e.target);
			hideTooltip(this, e.target);
		}
	});
	toolbar.bind('touchstart', function() {
		w2ui['spreadsheet-toolbar'].touchStarted = true;
	});

	toolbar = $('#presentation-toolbar');
	toolbar.w2toolbar({
		name: 'presentation-toolbar',
		tooltip: 'bottom',
		hidden: true,
		items: [
			{type: 'html',  id: 'left'},
			{type: 'button',  id: 'presentation', img: 'presentation', hidden:true, hint: _('Fullscreen presentation')},
			{type: 'break', id: 'presentationbreak', hidden:true},
			{type: 'button',  id: 'insertpage', img: 'insertpage', hint: _UNO('.uno:TaskPaneInsertPage', 'presentation')},
			{type: 'button',  id: 'duplicatepage', img: 'duplicatepage', hint: _UNO('.uno:DuplicateSlide', 'presentation')},
			{type: 'button',  id: 'deletepage', img: 'deletepage', hint: _UNO('.uno:DeleteSlide', 'presentation')},
			{type: 'html',  id: 'right'}
		],
		onClick: function (e) {
			onClick(e, e.target);
			hideTooltip(this, e.target);
		}
	});
	toolbar.bind('touchstart', function() {
		w2ui['presentation-toolbar'].touchStarted = true;
	});

	toolbar = $('#toolbar-down');
	if (!window.mode.isMobile()) {
		toolbar.w2toolbar({
			name: 'actionbar',
			tooltip: 'top',
			items: [
				{type: 'html',  id: 'search',
				 html: '<div style="padding: 3px 10px;" class="loleaflet-font">' +
				 ' ' + _('Search:') +
				 '    <input size="10" id="search-input"' +
				 'style="padding: 3px; border-radius: 2px; border: 1px solid silver"/>' +
				 '</div>'
				},
				{type: 'button',  id: 'searchprev', img: 'prev', hint: _UNO('.uno:UpSearch'), disabled: true},
				{type: 'button',  id: 'searchnext', img: 'next', hint: _UNO('.uno:DownSearch'), disabled: true},
				{type: 'button',  id: 'cancelsearch', img: 'cancel', hint: _('Cancel the search'), hidden: true},
				{type: 'html',  id: 'left'},
				{type: 'html',  id: 'right'},
				{type: 'html',  id: 'modifiedstatuslabel', hidden: true, html: '<div id="modifiedstatuslabel" class="loleaflet-font"></div>', mobile: false, tablet: false},
				{type: 'break', id: 'modifiedstatuslabelbreak', mobile: false},
				{type: 'drop', id: 'userlist', img: 'users', hidden: true, html: '<div id="userlist_container"><table id="userlist_table"><tbody></tbody></table>' +
					'<hr><table class="loleaflet-font" id="editor-btn">' +
					'<tr>' +
					'<td><input type="checkbox" name="alwaysFollow" id="follow-checkbox" onclick="editorUpdate(event)"></td>' +
					'<td>' + _('Always follow the editor') + '</td>' +
					'</tr>' +
					'</table>' +
					'<p id="currently-msg">' + _('Current') + ' - <b><span id="current-editor"></span></b></p>' +
					'</div>'
				},
				{type: 'break', id: 'userlistbreak', hidden: true, mobile: false },
				{type: 'button',  id: 'prev', img: 'prev', hint: _UNO('.uno:PageUp', 'text')},
				{type: 'button',  id: 'next', img: 'next', hint: _UNO('.uno:PageDown', 'text')},
				{type: 'break', id: 'prevnextbreak'},
				{type: 'button',  id: 'zoomreset', img: 'zoomreset', hint: _('Reset zoom')},
				{type: 'button',  id: 'zoomout', img: 'zoomout', hint: _UNO('.uno:ZoomMinus')},
				{type: 'html',    id: 'zoomlevel', html: '<div id="zoomlevel" class="loleaflet-font">100%</div>', mobile: false},
				{type: 'button',  id: 'zoomin', img: 'zoomin', hint: _UNO('.uno:ZoomPlus')}
			],
			onClick: function (e) {
				hideTooltip(this, e.target);
				if (e.item.id === 'userlist') {
					setTimeout(function() {
						var cBox = $('#follow-checkbox')[0];
						var docLayer = map._docLayer;
						var editorId = docLayer._editorId;

						if (cBox)
							cBox.checked = docLayer._followEditor;

						if (docLayer.editorId !== -1 && map._viewInfo[editorId])
							$('#current-editor').text(map._viewInfo[editorId].username);
						else
							$('#currently-msg').hide();
					}, 100);
					return;
				}
				onClick(e, e.target, e.item, e.subItem);
			},
			onRefresh: function() {
				$('#tb_actionbar_item_userlist .w2ui-tb-caption').addClass('loleaflet-font');
				$('#search-input').off('input', onSearch).on('input', onSearch);
				$('#search-input').off('keydown', onSearchKeyDown).on('keydown', onSearchKeyDown);

				var showInDesktop = map['wopi'].HideUserList !== null &&
									map['wopi'].HideUserList !== undefined &&
									$.inArray('true', map['wopi'].HideUserList) < 0 &&
									$.inArray('desktop', map['wopi'].HideUserList) < 0;
				if (this.get('userlist').hidden == true && showInDesktop) {
					this.show('userlist');
					this.show('userlistbreak');
					map.on('deselectuser', deselectUser);
					map.on('addview', onAddView);
					map.on('removeview', onRemoveView);
				}
			}
		});
	}
	else {
		toolbar.w2toolbar({
			name: 'actionbar',
			tooltip: 'top',
			items: []
		});
	}
	toolbar.bind('touchstart', function() {
		w2ui['actionbar'].touchStarted = true;
	});
}

var userJoinedPopupMessage = '<div>' + _('%user has joined') + '</div>';
var userLeftPopupMessage = '<div>' + _('%user has left') + '</div>';
var userPopupTimeout = null;

function localizeStateTableCell (text) {
	var stateArray = text.split(';');
	var stateArrayLength = stateArray.length;
	var localizedText = '';
	for (var i = 0; i < stateArrayLength; i++) {
		var labelValuePair = stateArray[i].split(':');
		localizedText += _(labelValuePair[0].trim()) + ':' + labelValuePair[1];
		if (stateArrayLength > 1 && i < stateArrayLength - 1) {
			localizedText += '; ';
		}
	}
	return localizedText;
}

function toLocalePattern (pattern, regex, text, sub1, sub2) {
	var matches = new RegExp(regex, 'g').exec(text);
	if (matches) {
		text = pattern.toLocaleString().replace(sub1, parseInt(matches[1].replace(',','')).toLocaleString(String.locale)).replace(sub2, parseInt(matches[2].replace(',','')).toLocaleString(String.locale));
	}
	return text;
}

function updateToolbarItem(toolbar, id, html) {
	var item = toolbar.get(id);
	if (item) {
		item.html = html;
	}
}

function unoCmdToToolbarId(commandname)
{
	var id = commandname.toLowerCase().substr(5);
	if (map.getDocType() === 'spreadsheet') {
		switch (id) {
		case 'alignleft':
			id = 'leftpara';
			break;
		case 'alignhorizontalcenter':
			id = 'centerpara';
			break;
		case 'alignright':
			id = 'rightpara';
			break;
		}
	}
	return id;
}

function onSearch() {
	var toolbar = w2ui['actionbar'];
	// conditionally disabling until, we find a solution for tdf#108577
	if (L.DomUtil.get('search-input').value === '') {
		toolbar.disable('searchprev');
		toolbar.disable('searchnext');
		toolbar.hide('cancelsearch');
	}
	else {
		if (map.getDocType() === 'text')
			map.search(L.DomUtil.get('search-input').value, false, '', 0, true /* expand search */);
		toolbar.enable('searchprev');
		toolbar.enable('searchnext');
		toolbar.show('cancelsearch');
	}
}

function onSearchKeyDown(e) {
	if ((e.keyCode === 71 && e.ctrlKey) || e.keyCode === 114 || e.keyCode === 13) {
		if (e.shiftKey) {
			map.search(L.DomUtil.get('search-input').value, true);
		} else {
			map.search(L.DomUtil.get('search-input').value);
		}
		e.preventDefault();
	} else if (e.keyCode === 27) {
		_cancelSearch();
	}
}

function documentNameConfirm() {
	var value = $('#document-name-input').val();
	if (value !== null && value != '' && value != map['wopi'].BaseFileName) {
		map.saveAs(value);
	}
	map._onGotFocus();
}

function documentNameCancel() {
	$('#document-name-input').val(map['wopi'].BaseFileName);
	map._onGotFocus();
}

function onDocumentNameKeyPress(e) {
	if (e.keyCode === 13) { // Enter key
		documentNameConfirm();
	} else if (e.keyCode === 27) { // Escape key
		documentNameCancel();
	}
}

function onDocumentNameFocus() {
	// hide the caret in the main document
	map._onLostFocus();
}

function sortFontSizes() {
	var oldVal = $('.fontsizes-select').val();
	var selectList = $('.fontsizes-select option');
	selectList.sort(function (a, b) {
		a = parseFloat($(a).text() * 1);
		b = parseFloat($(b).text() * 1);
		if (a > b) {
			return 1;
		} else if (a < b) {
			return -1;
		}
		return 0;
	});
	$('.fontsizes-select').html(selectList);
	$('.fontsizes-select').val(oldVal).trigger('change');
}

function onStyleSelect(e) {
	var style = e.target.value;
	if (style.startsWith('.uno:')) {
		map.sendUnoCommand(style);
	}
	else if (map.getDocType() === 'text') {
		map.applyStyle(style, 'ParagraphStyles');
	}
	else if (map.getDocType() === 'spreadsheet') {
		map.applyStyle(style, 'CellStyles');
	}
	else if (map.getDocType() === 'presentation' || map.getDocType() === 'drawing') {
		map.applyLayout(style);
	}
	map.focus();
}

function updateFontSizeList(font) {
	var oldSize = $('.fontsizes-select').val();
	var found = false;
	$('.fontsizes-select').find('option').remove();
	var data = [''];
	data = data.concat(map.getToolbarCommandValues('.uno:CharFontName')[font]);
	$('.fontsizes-select').select2({
		data: data,
		placeholder: ' ',
		//Allow manually entered font size.
		createTag: function(query) {
			return {
				id: query.term,
				text: query.term,
				tag: true
			};
		},
		tags: true
	});
	$('.fontsizes-select option').each(function (i, e) {
		if ($(e).text() === oldSize) {
			$('.fontsizes-select').val(oldSize).trigger('change');
			found = true;
			return;
		}
	});
	if (!found) {
		// we need to add the size
		$('.fontsizes-select')
			.append($('<option></option>')
			.text(oldSize));
	}
	$('.fontsizes-select').val(oldSize).trigger('change');
	sortFontSizes();
}

function onFontSelect(e) {
	var font = e.target.value;
	updateFontSizeList(font);
	map.applyFont(font);
	map.focus();
}

function onFontSizeSelect(e) {
	var size = e.target.value;
	var command = {};
	$(e.target).find('option[data-select2-tag]').removeAttr('data-select2-tag');
	map.applyFontSize(size);
	var fontcolor = map.getDocType() === 'text' ? 'FontColor' : 'Color';
	command[fontcolor] = {};
	map.focus();
}

function onInsertFile() {
	var insertGraphic = L.DomUtil.get('insertgraphic');
	if ('files' in insertGraphic) {
		for (var i = 0; i < insertGraphic.files.length; i++) {
			var file = insertGraphic.files[i];
			map.insertFile(file);
		}
	}

	// Set the value to null everytime so that onchange event is triggered,
	// even if the same file is selected
	insertGraphic.value = null;
	return false;
}

function onAddressInput(e) {
	if (e.keyCode === 13) {
		// address control should not have focus anymore
		map.focus();
		var value = L.DomUtil.get('addressInput').value;
		var command = {
			ToPoint : {
				type: 'string',
				value: value
			}

		};
		map.sendUnoCommand('.uno:GoToCell', command);
	} else if (e.keyCode === 27) { // 27 = esc key
		map.sendUnoCommand('.uno:Cancel');
		map.focus();
	}
}

function onFormulaInput(e) {
	// keycode = 13 is 'enter'
	if (e.keyCode === 13) {
		// formula bar should not have focus anymore
		map.focus();

		// forward the 'enter' keystroke to map to deal with the formula entered
		var data = {
			originalEvent: e
		};
		map.fire('keypress', data);
	} else if (e.keyCode === 27) { // 27 = esc key
		map.sendUnoCommand('.uno:Cancel');
		map.focus();
	} else {
		map.cellEnterString(L.DomUtil.get('formulaInput').value);
	}
}

function onFormulaBarFocus() {
	var formulabar = w2ui.formulabar;
	formulabar.hide('sum');
	formulabar.hide('function');
	formulabar.show('cancelformula');
	formulabar.show('acceptformula');
}

function onFormulaBarBlur() {
	// The timeout is needed because we want 'click' event on 'cancel',
	// 'accept' button to act before we hide these buttons because
	// once hidden, click event won't be processed.
	// TODO: Some better way to do it ?
	setTimeout(function() {
		var formulabar = w2ui.formulabar;
		formulabar.show('sum');
		formulabar.show('function');
		formulabar.hide('cancelformula');
		formulabar.hide('acceptformula');
	}, 250);
}



function onWopiProps(e) {
	if (e.HideSaveOption) {
		w2ui['editbar'].hide('save');
	}
	if (e.HideExportOption) {
		w2ui['presentation-toolbar'].hide('presentation', 'presentationbreak');
	}
	if (e.HidePrintOption) {
		w2ui['editbar'].hide('print');
	}
	if (e.DisableCopy) {
		$('input#formulaInput').bind('copy', function(evt) {
			evt.preventDefault();
		});
		$('input#addressInput').bind('copy', function(evt) {
			evt.preventDefault();
		});
	}
	if (e.BaseFileName !== null) {
		// set the document name into the name field
		$('#document-name-input').val(e.BaseFileName);
	}
	if (e.UserCanNotWriteRelative === false) {
		// Save As allowed
		$('#document-name-input').prop('disabled', false);
		$('#document-name-input').addClass('editable');
		$('#document-name-input').off('keypress', onDocumentNameKeyPress).on('keypress', onDocumentNameKeyPress);
		$('#document-name-input').off('focus', onDocumentNameFocus).on('focus', onDocumentNameFocus);
		$('#document-name-input').off('blur', documentNameCancel).on('blur', documentNameCancel);
	} else {
		$('#document-name-input').prop('disabled', true);
		$('#document-name-input').removeClass('editable');
		$('#document-name-input').off('keypress', onDocumentNameKeyPress);
	}
	if (e.EnableInsertRemoteImage === true) {
		w2ui['editbar'].hide('insertgraphic');
		w2ui['editbar'].show('menugraphic');
	}
}

function onDocLayerInit() {
	var toolbarUp = w2ui['editbar'];
	var statusbar = w2ui['actionbar'];
	var docType = map.getDocType();

	switch (docType) {
	case 'spreadsheet':
		toolbarUp.show('textalign', 'wraptext', 'breakspacing', 'insertannotation', 'conditionalformaticonset',
			'numberformatcurrency', 'numberformatpercent',
			'numberformatincdecimals', 'numberformatdecdecimals', 'break-number', 'togglemergecells', 'breakmergecells',
			'setborderstyle', 'sortascending', 'sortdescending', 'breaksorting');
		toolbarUp.remove('styles');

		statusbar.remove('prev', 'next', 'prevnextbreak');

		toolbarUp.set('zoom', {
			items: [
				{ id: 'zoom100', text: '100%', scale: 10},
				{ id: 'zoom200', text: '200%', scale: 14}
			]
		});

		if (!_inMobileMode()) {
			statusbar.insert('left', [
				{type: 'break', id: 'break1'},
				{
					type: 'html', id: 'StatusDocPos',
					html: '<div id="StatusDocPos" class="loleaflet-font" title="' + _('Number of Sheets') + '" style="padding: 5px 5px;">&nbsp;&nbsp;&nbsp;&nbsp;&nbsp</div>'
				},
				{type: 'break', id: 'break2'},
				{
					type: 'html', id: 'RowColSelCount',
					html: '<div id="RowColSelCount" class="loleaflet-font" title="' + _('Selected range of cells') + '" style="padding: 5px 5px;">&nbsp;&nbsp;&nbsp;&nbsp;&nbsp</div>'
				},
				{type: 'break', id: 'break3', tablet: false},
				{
					type: 'html', id: 'InsertMode', mobile: false, tablet: false,
					html: '<div id="InsertMode" class="loleaflet-font" title="' + _('Entering text mode') + '" style="padding: 5px 5px;">&nbsp;&nbsp;&nbsp;&nbsp;&nbsp</div>'
				},
				{type: 'break', id: 'break4', tablet: false},
				{
					type: 'html', id: 'LanguageStatus', mobile: false, tablet: false,
					html: '<div id="LanguageStatus" class="loleaflet-font" title="' + _('Text Language') + '" style="padding: 5px 5px;">&nbsp;&nbsp;&nbsp;&nbsp;&nbsp</div>'
				},
				{type: 'break', id: 'break5', tablet: false},
				{
					type: 'html', id: 'StatusSelectionMode', mobile: false, tablet: false,
					html: '<div id="StatusSelectionMode" class="loleaflet-font" title="' + _('Selection Mode') + '" style="padding: 5px 5px;">&nbsp;&nbsp;&nbsp;&nbsp;&nbsp</div>'
				},
				{type: 'break', id: 'break8', mobile: false, tablet: false},
				{
					type: 'html', id: 'StateTableCell', mobile: false, tablet: false,
					html: '<div id="StateTableCell" class="loleaflet-font" title="' + _('Choice of functions') + '" style="padding: 5px 5px;">&nbsp;&nbsp;&nbsp;&nbsp;&nbsp</div>'
				},
				{
					type: 'menu-check', id: 'StateTableCellMenu', caption: '', selected: ['2', '512'], items: [
						{id: '2', text: _('Average')},
						{id: '8', text: _('CountA')},
						{id: '4', text: _('Count')},
						{id: '16', text: _('Maximum')},
						{id: '32', text: _('Minimum')},
						{id: '512', text: _('Sum')},
						{id: '8192', text: _('Selection count')},
						{id: '1', text: _('None')}
					], tablet: false
				},
				{type: 'break', id: 'break8', mobile: false}
			]);

			$('#spreadsheet-toolbar').show();
		}
		$('#formulabar').show();

		break;
	case 'text':
		toolbarUp.show('leftpara', 'centerpara', 'rightpara', 'justifypara', 'breakpara', 'linespacing',
			'breakspacing', 'defaultbullet', 'defaultnumbering', 'breakbullet', 'incrementindent', 'decrementindent',
			'breakindent', 'inserttable', 'insertannotation');

		if (!_inMobileMode()) {
			statusbar.insert('left', [
				{type: 'break', id: 'break1'},
				{
					type: 'html', id: 'StatePageNumber',
					html: '<div id="StatePageNumber" class="loleaflet-font" title="' + _('Number of Pages') + '" style="padding: 5px 5px;">&nbsp;&nbsp;&nbsp;&nbsp;&nbsp</div>'
				},
				{type: 'break', id: 'break2'},
				{
					type: 'html', id: 'StateWordCount', mobile: false, tablet: false,
					html: '<div id="StateWordCount" class="loleaflet-font" title="' + _('Word Counter') + '" style="padding: 5px 5px;">&nbsp;&nbsp;&nbsp;&nbsp;&nbsp</div>'
				},
				{type: 'break', id: 'break5', mobile: false, tablet: false},
				{
					type: 'html', id: 'InsertMode', mobile: false, tablet: false,
					html: '<div id="InsertMode" class="loleaflet-font" title="' + _('Entering text mode') + '" style="padding: 5px 5px;">&nbsp;&nbsp;&nbsp;&nbsp;&nbsp</div>'
				},
				{type: 'break', id: 'break6', mobile: false, tablet: false},
				{
					type: 'html', id: 'StatusSelectionMode', mobile: false, tablet: false,
					html: '<div id="StatusSelectionMode" class="loleaflet-font" title="' + _('Selection Mode') + '" style="padding: 5px 5px;">&nbsp;&nbsp;&nbsp;&nbsp;&nbsp</div>'
				},
				{type: 'break', id: 'break7', mobile: false, tablet: false},
				{
					type: 'html', id: 'LanguageStatus', mobile: false, tablet: false,
					html: '<div id="LanguageStatus" class="loleaflet-font" title="' + _('Text Language') + '" style="padding: 5px 5px;">&nbsp;&nbsp;&nbsp;&nbsp;&nbsp</div>'
				},
				{type: 'break', id: 'break8', mobile: false}
			]);
		}

		break;
	case 'presentation':
		var presentationToolbar = w2ui['presentation-toolbar'];
		if (!map['wopi'].HideExportOption) {
			presentationToolbar.show('presentation', 'presentationbreak');
		}
		if (!_inMobileMode()) {
			statusbar.insert('left', [
				{type: 'break', id: 'break1'},
				{
					type: 'html', id: 'PageStatus',
					html: '<div id="PageStatus" class="loleaflet-font" title="' + _('Number of Slides') + '" style="padding: 5px 5px;">&nbsp;&nbsp;&nbsp;&nbsp;&nbsp</div>'
				},
				{type: 'break', id: 'break2', mobile: false, tablet: false},
				{
					type: 'html', id: 'LanguageStatus', mobile: false, tablet: false,
					html: '<div id="LanguageStatus" class="loleaflet-font" title="' + _('Text Language') + '" style="padding: 5px 5px;">&nbsp;&nbsp;&nbsp;&nbsp;&nbsp</div>'
				},
				{type: 'break', id: 'break8', mobile: false}
			]);
		}
		// FALLTHROUGH intended
	case 'drawing':
		toolbarUp.show('leftpara', 'centerpara', 'rightpara', 'justifypara', 'breakpara', 'linespacing',
			'breakspacing', 'defaultbullet', 'defaultnumbering', 'breakbullet', 'inserttable');
		statusbar.show('prev', 'next');

		$('#presentation-toolbar').show();
		break;
	}

	if (L.DomUtil.get('document-signing-bar') !== null) {
		map.signingInitializeBar();
	}

	_updateToolbarsVisibility();

	if (L.Browser.mobile) {
		nUsers = '%n';
		oneUser = '1';
		noUser = '0';
		$('#document-name-input').hide();
	} else {
		nUsers = _('%n users');
		oneUser = _('1 user');
		noUser = _('0 users');
		$('#document-name-input').show();
	}

	updateUserListCount();
	toolbarUp.refresh();
	statusbar.refresh();

	if (window.mode.isTablet()) {
		// Fold menubar by default
		// FIXME: reuse toogleMenubar / use css
		$('.main-nav').css({'display': 'none'});
		$('#closebuttonwrapper').css({'display': 'none'});
		var obj = $('.fold');
		obj.removeClass('w2ui-icon fold');
		obj.addClass('w2ui-icon unfold');
		$('#document-container').addClass('tablet');
		$('#spreadsheet-row-column-frame').addClass('tablet');
		$('#presentation-controls-wrapper').css({'top': '41px'});

		$('#tb_editbar_item_fonts').css({'display': 'none'});
		$('#tb_editbar_item_fontsizes').css({'display': 'none'});
	}

	if (docType == 'spreadsheet') {
		var el = w2ui['spreadsheet-toolbar'];
		if (el)
			el.resize();
	}
}

function onCommandStateChanged(e) {
	var toolbar = w2ui['editbar'];
	var statusbar = w2ui['actionbar'];
	var commandName = e.commandName;
	var state = e.state;
	var found = false;
	var value, color, div;

	if (commandName === '.uno:AssignLayout') {
		$('.styles-select').val(state).trigger('change');
	} else if (commandName === '.uno:StyleApply') {
		if (!state) {
			return;
		}

		// For impress documents, no styles is supported.
		if (map.getDocType() === 'presentation') {
			return;
		}

		$('.styles-select option').each(function () {
			var value = this.value;
			// For writer we get UI names; ideally we should be getting only programmatic ones
			// For eg: 'Text body' vs 'Text Body'
			// (likely to be fixed in core to make the pattern consistent)
			if (state && value.toLowerCase() === state.toLowerCase()) {
				state = value;
				found = true;
				return;
			}
		});
		if (!found) {
			// we need to add the size
			$('.styles-select')
				.append($('<option></option>')
				.text(state));
		}

		stylesSelectValue = state;
		$('.styles-select').val(state).trigger('change');
	}
	else if (commandName === '.uno:CharFontName') {
		$('.fonts-select option').each(function () {
			value = this.value;
			if (value.toLowerCase() === state.toLowerCase()) {
				found = true;
				updateFontSizeList(value);
				return;
			}
		});
		if (!found) {
			// we need to add the size
			$('.fonts-select')
				.append($('<option></option>')
				.text(state));
		}
		fontsSelectValue = state;
		$('.fonts-select').val(state).trigger('change');
	}
	else if (commandName === '.uno:FontHeight') {
		if (state === '0') {
			state = '';
		}
		$('.fontsizes-select option').each(function (i, e) {
			if ($(e).text() === state) {
				found = true;
				return;
			}
		});
		if (!found) {
			// we need to add the size
			$('.fontsizes-select')
				.append($('<option></option>')
				.text(state).val(state));
		}
		fontsizesSelectValue = state;
		$('.fontsizes-select').val(state).trigger('change');
		sortFontSizes();
	}
	else if (commandName === '.uno:FontColor' || commandName === '.uno:Color') {
		// confusingly, the .uno: command is named differently in Writer, Calc and Impress
		color = parseInt(e.state);
		if (color === -1) {
			color = 'transparent';
		}
		else {

			color = color.toString(16);
			color = '#' + Array(7 - color.length).join('0') + color;
		}
		div = L.DomUtil.get('fontcolorindicator');
		if (div) {
			L.DomUtil.setStyle(div, 'background', color);
		}
	}
	else if (commandName === '.uno:BackColor' || commandName === '.uno:BackgroundColor' || commandName === '.uno:CharBackColor') {
		// confusingly, the .uno: command is named differently in Writer, Calc and Impress
		color = parseInt(e.state);
		if (color === -1) {
			color = 'transparent';
		}
		else {
			color = color.toString(16);
			color = '#' + Array(7 - color.length).join('0') + color;
		}
		div = L.DomUtil.get('backcolorindicator');
		if (div) {
			L.DomUtil.setStyle(div, 'background', color);
		}
	}
	else if (commandName === '.uno:LanguageStatus') {
		updateToolbarItem(statusbar, 'LanguageStatus', $('#LanguageStatus').html(_(state)).parent().html());
	}
	else if (commandName === '.uno:ModifiedStatus') {
		var modifiedStatus = e.state === 'true';
		var html;
		if (modifiedStatus) {
			html = $('#modifiedstatuslabel').html('').parent().html();
			w2ui['editbar'].set('save', {img:'savemodified'});
		}
		else {
			html = $('#modifiedstatuslabel').html(_('Document saved')).parent().html();
			w2ui['editbar'].set('save', {img:'save'});
		}
		updateToolbarItem(statusbar, 'modifiedstatuslabel', html);
	}
	else if (commandName === '.uno:StatusDocPos') {
		state = toLocalePattern('Sheet %1 of %2', 'Sheet (\\d+) of (\\d+)', state, '%1', '%2');
		updateToolbarItem(statusbar, 'StatusDocPos', $('#StatusDocPos').html(state ? state : '&nbsp;&nbsp;&nbsp;&nbsp;&nbsp').parent().html());
	}
	else if (commandName === '.uno:RowColSelCount') {
		state = toLocalePattern('$1 rows, $2 columns selected', '(\\d+) rows, (\\d+) columns selected', state, '$1', '$2');
		state = toLocalePattern('$1 of $2 records found', '(\\d+) of (\\d+) records found', state, '$1', '$2');
		updateToolbarItem(statusbar, 'RowColSelCount', $('#RowColSelCount').html(state ? state : '&nbsp;&nbsp;&nbsp;&nbsp;&nbsp').parent().html());
	}
	else if (commandName === '.uno:InsertMode') {
		updateToolbarItem(statusbar, 'InsertMode', $('#InsertMode').html(state ? L.Styles.insertMode[state].toLocaleString() : '&nbsp;&nbsp;&nbsp;&nbsp;&nbsp').parent().html());
	}
	else if (commandName === '.uno:StatusSelectionMode' ||
		 commandName === '.uno:SelectionMode') {
		updateToolbarItem(statusbar, 'StatusSelectionMode', $('#StatusSelectionMode').html(state ? L.Styles.selectionMode[state].toLocaleString() : '&nbsp;&nbsp;&nbsp;&nbsp;&nbsp').parent().html());
	}
	else if (commandName == '.uno:StateTableCell') {
		updateToolbarItem(statusbar, 'StateTableCell', $('#StateTableCell').html(state ? localizeStateTableCell(state) : '&nbsp;&nbsp;&nbsp;&nbsp;&nbsp').parent().html());
	}
	else if (commandName === '.uno:StatusBarFunc') {
		var item = statusbar.get('StateTableCellMenu');
		if (item) {
			item.selected = [];
			// Check 'None' even when state is 0
			if (state === '0') {
				state = 1;
			}
			for (var it = 0; it < item.items.length; it++) {
				if (item.items[it].id & state) {
					item.selected.push(item.items[it].id);
				}
			}
		}
	}
	else if (commandName === '.uno:StatePageNumber') {
		state = toLocalePattern('Page %1 of %2', 'Page (\\d+) of (\\d+)', state, '%1', '%2');
		updateToolbarItem(statusbar, 'StatePageNumber', $('#StatePageNumber').html(state ? state : '&nbsp;&nbsp;&nbsp;&nbsp;&nbsp').parent().html());
	}
	else if (commandName === '.uno:StateWordCount') {
		state = toLocalePattern('%1 words, %2 characters', '([\\d,]+) words, ([\\d,]+) characters', state, '%1', '%2');
		updateToolbarItem(statusbar, 'StateWordCount', $('#StateWordCount').html(state ? state : '&nbsp;&nbsp;&nbsp;&nbsp;&nbsp').parent().html());
	}
	else if (commandName === '.uno:PageStatus') {
		state = toLocalePattern('Slide %1 of %2', 'Slide (\\d+) of (\\d+)', state, '%1', '%2');
		updateToolbarItem(statusbar, 'PageStatus', $('#PageStatus').html(state ? state : '&nbsp;&nbsp;&nbsp;&nbsp;&nbsp').parent().html());
	}
	else if (commandName === '.uno:DocumentRepair') {
		if (state === 'true') {
			toolbar.enable('repair');
		} else {
			toolbar.disable('repair');
		}
	}

	var id = unoCmdToToolbarId(commandName);
	if (state === 'true') {
		if (map._permission === 'edit') {
			toolbar.enable(id);
		}
		toolbar.check(id);
	}
	else if (state === 'false') {
		if (map._permission === 'edit') {
			toolbar.enable(id);
		}
		toolbar.uncheck(id);
	}
	// Change the toolbar button states if we are in editmode
	// If in non-edit mode, will be taken care of when permission is changed to 'edit'
	else if (map._permission === 'edit' && (state === 'enabled' || state === 'disabled')) {
		var toolbarUp = toolbar;
		if (_inMobileMode()) {
			toolbarUp = statusbar;
		}
		if (state === 'enabled') {
			toolbarUp.enable(id);
		} else {
			toolbarUp.uncheck(id);
			toolbarUp.disable(id);
		}
	}
}

function updateCommandValues(targetName) {
	var data = [];
	// 1) For .uno:StyleApply
	// we need an empty option for the place holder to work
	if (targetName === 'styles' && $('.styles-select option').length === 1) {
		var styles = [];
		var topStyles = [];
		var commandValues = map.getToolbarCommandValues('.uno:StyleApply');
		if (typeof commandValues === 'undefined')
			return;
		var commands = commandValues.Commands;
		if (commands && commands.length > 0) {
			// Inserts a separator element
			data = data.concat({text: '\u2500\u2500\u2500\u2500\u2500\u2500', disabled: true});

			commands.forEach(function (command) {
				var translated = command.text;
				if (L.Styles.styleMappings[command.text]) {
					// if it's in English, translate it
					translated = L.Styles.styleMappings[command.text].toLocaleString();
				}
				data = data.concat({id: command.id, text: translated });
			}, this);
		}

		if (map.getDocType() === 'text') {
			styles = commandValues.ParagraphStyles.slice(7, 19);
			topStyles = commandValues.ParagraphStyles.slice(0, 7);
		}
		else if (map.getDocType() === 'spreadsheet') {
			styles = commandValues.CellStyles;
		}
		else if (map.getDocType() === 'presentation') {
			// styles are not applied for presentation
			return;
		}

		if (topStyles.length > 0) {
			// Inserts a separator element
			data = data.concat({text: '\u2500\u2500\u2500\u2500\u2500\u2500', disabled: true});

			topStyles.forEach(function (style) {
				data = data.concat({id: style, text: L.Styles.styleMappings[style].toLocaleString()});
			}, this);
		}

		if (styles !== undefined && styles.length > 0) {
			// Inserts a separator element
			data = data.concat({text: '\u2500\u2500\u2500\u2500\u2500\u2500', disabled: true});

			styles.forEach(function (style) {
				var localeStyle;
				if (style.startsWith('outline')) {
					var outlineLevel = style.split('outline')[1];
					localeStyle = 'Outline'.toLocaleString() + ' ' + outlineLevel;
				} else {
					localeStyle = L.Styles.styleMappings[style];
					localeStyle = localeStyle === undefined ? style : localeStyle.toLocaleString();
				}

				data = data.concat({id: style, text: localeStyle});
			}, this);
		}

		$('.styles-select').select2({
			data: data,
			placeholder: _('Style')
		});
		$('.styles-select').val(stylesSelectValue).trigger('change');
		$('.styles-select').on('select2:select', onStyleSelect);
		w2ui['editbar'].resize();
	}

	if (targetName === 'fonts' && $('.fonts-select option').length === 1) {
		// 2) For .uno:CharFontName
		commandValues = map.getToolbarCommandValues('.uno:CharFontName');
		if (typeof commandValues === 'undefined') {
			return;
		}
		data = []; // reset data in order to avoid that the font select box is populated with styles, too.
		// Old browsers like IE11 et al don't like Object.keys with
		// empty arguments
		if (typeof commandValues === 'object') {
			data = data.concat(Object.keys(commandValues));
		}
		$('.fonts-select').select2({
			data: data.sort(function (a, b) {  // also sort(localely)
				return a.localeCompare(b);
			}),
			placeholder: _('Font')
		});
		$('.fonts-select').on('select2:select', onFontSelect);
		$('.fonts-select').val(fontsSelectValue).trigger('change');
		w2ui['editbar'].resize();
	}

	if (targetName === 'fontsizes' && $('.fontsizes-select option').length === 1) {
		$('.fontsizes-select').select2({
			placeholder: ' ',
			data: []
		});

		$('.fontsizes-select').on('select2:select', onFontSizeSelect);
		if (fontsSelectValue) {
			updateFontSizeList(fontsSelectValue);
		}
		$('.fontsizes-select').val(fontsizesSelectValue).trigger('change');
		w2ui['editbar'].resize();
	}
}


function onUpdateParts(e) {
	if (e.docType === 'text') {
		var current = e.currentPage;
		var count = e.pages;
	}
	else {
		current = e.selectedPart;
		count = e.parts;
	}

	var toolbar = w2ui['actionbar'];
	if (e.docType === 'presentation') {
		toolbar.set('prev', {hint: _('Previous slide')});
		toolbar.set('next', {hint: _('Next slide')});
	}
	else {
		toolbar.hide('presentation');
		toolbar.hide('insertpage');
		toolbar.hide('duplicatepage');
		toolbar.hide('deletepage');
	}

	if (e.docType !== 'spreadsheet') {
		if (current === 0) {
			toolbar.disable('prev');
		}
		else {
			toolbar.enable('prev');
		}

		if (current === count - 1) {
			toolbar.disable('next');
		}
		else {
			toolbar.enable('next');
		}
	}
}

function onCommandResult(e) {
	var commandName = e.commandName;

	if (commandName === '.uno:Save') {
		if (e.success) {
			// Saved a new version; the document is modified.
			map._everModified = true;
		}
		var postMessageObj = {
			success: e.success
		};
		if (!e.success) {
			// add the result reason string if failed
			postMessageObj['result'] = e.result && e.result.value;
		}
		map.fire('postMessage', {msgId: 'Action_Save_Resp', args: postMessageObj});
	}
	else if ((commandName === '.uno:Undo' || commandName === '.uno:Redo') &&
		e.success === true && e.result.value && !isNaN(e.result.value)) { /*UNDO_CONFLICT*/
		$('#tb_editbar_item_repair').w2overlay({ html: '<div style="padding: 10px; line-height: 150%">' +
			_('Conflict Undo/Redo with multiple users. Please use document repair to resolve') + '</div>'});
	}
}

function onUpdatePermission(e) {
	var toolbar = w2ui['editbar'];

	// always enabled items
	var enabledButtons = ['closemobile', 'undo', 'redo'];

	// copy the first array
	var items = toolbar.items.slice();
	for (var idx in items) {
		var found = enabledButtons.filter(function(id) { return id === items[idx].id });
		var alwaysEnable = found.length !== 0;

		if (e.perm === 'edit') {
			var unoCmd = map.getDocType() === 'spreadsheet' ? items[idx].unosheet : items[idx].uno;
			var keepDisabled = map['stateChangeHandler'].getItemValue(unoCmd) === 'disabled';
			if (!keepDisabled || alwaysEnable) {
				toolbar.enable(items[idx].id);
			}
		} else if (!alwaysEnable) {
			toolbar.disable(items[idx].id);
		}
	}

	var spreadsheetButtons = ['insertsheet'];
	var formulaBarButtons = ['sum', 'function'];
	var presentationButtons = ['insertpage', 'duplicatepage', 'deletepage'];
	var toolbarDownButtons = ['next', 'prev'];
	if (e.perm === 'edit') {
		// Enable list boxes
		$('.styles-select').prop('disabled', false);
		$('.fonts-select').prop('disabled', false);
		$('.fontsizes-select').prop('disabled', false);

		// Enable formula bar
		$('#addressInput').prop('disabled', false);
		$('#formulaInput').prop('disabled', false);
		toolbar = w2ui.formulabar;
		formulaBarButtons.forEach(function(id) {
			toolbar.enable(id);
		});

		toolbar = w2ui['spreadsheet-toolbar'];
		spreadsheetButtons.forEach(function(id) {
			toolbar.enable(id);
		});

		toolbar = w2ui['presentation-toolbar'];
		presentationButtons.forEach(function(id) {
			toolbar.enable(id);
		});

		toolbar = w2ui['actionbar'];
		toolbarDownButtons.forEach(function(id) {
			toolbar.enable(id);
		});
		$('#search-input').prop('disabled', false);

		// FIXME avoid hardcoding this stuff if possible
		if (_inMobileMode()) {
			$('#toolbar-down').show();
			switch (map._docLayer._docType) {
			case 'text':
				$('#document-container').css('bottom', '33px');
				break;
			case 'spreadsheet':
				$('#document-container').css('bottom', '68px'); // FIXME this and spreadsheet-row-column-frame are supposed to be the same, but are not
				$('#spreadsheet-row-column-frame').css('bottom', '65px');
				$('#spreadsheet-toolbar').show();
				break;
			case 'presentation':
				$('#document-container').css('bottom', '33px');
				break;
			}
		}
	}
	else {
		// Disable list boxes
		$('.styles-select').prop('disabled', true);
		$('.fonts-select').prop('disabled', true);
		$('.fontsizes-select').prop('disabled', true);

		// Disable formula bar
		$('#addressInput').prop('disabled', true);
		$('#formulaInput').prop('disabled', true);

		toolbar = w2ui.formulabar;
		formulaBarButtons.forEach(function(id) {
			toolbar.disable(id);
		});

		toolbar = w2ui['spreadsheet-toolbar'];
		spreadsheetButtons.forEach(function(id) {
			toolbar.disable(id);
		});

		toolbar = w2ui['presentation-toolbar'];
		presentationButtons.forEach(function(id) {
			toolbar.disable(id);
		});

		toolbar = w2ui['actionbar'];
		toolbarDownButtons.forEach(function(id) {
			toolbar.disable(id);
		});
		$('#search-input').prop('disabled', true);

		// FIXME avoid hardcoding this stuff if possible
		if (_inMobileMode()) {
			$('#toolbar-down').hide();
			switch (map._docLayer._docType) {
			case 'text':
				$('#document-container').css('bottom', '0');
				break;
			case 'spreadsheet':
				$('#document-container').css('bottom', '35px');
				$('#spreadsheet-row-column-frame').css('bottom', '0');
				$('#spreadsheet-toolbar').show();
				break;
			case 'presentation':
				$('#document-container').css('bottom', '0');
				break;
			}
		}
	}
}

function onUseritemClicked(e) { // eslint-disable-line no-unused-vars
	var docLayer = map._docLayer;
	var viewId = parseInt(e.currentTarget.id.replace('user-', ''));

	map._goToViewId(viewId);

	if (viewId === map._docLayer._viewId) {
		$('#tb_actionbar_item_userlist').w2overlay('');
		return;
	} else if (docLayer._followThis !== -1) {
		map._setFollowing(false, null);
	}

	docLayer._followThis = viewId;
	docLayer._followUser = true;
	docLayer._followEditor = false;

	selectUser(viewId);
}

global.onUseritemClicked = onUseritemClicked;

function editorUpdate(e) { // eslint-disable-line no-unused-vars
	var docLayer = map._docLayer;

	if (e.target.checked) {
		var editorId = docLayer._editorId;

		docLayer._followUser = false;
		docLayer._followEditor = true;
		if (editorId !== -1 && editorId !== docLayer.viewId) {
			map._goToViewId(editorId);
			docLayer._followThis = editorId;
		}

		var userlistItem = w2ui['actionbar'].get('userlist');
		if (userlistItem !== null) {
			$('.selected-user').removeClass('selected-user');
			if ($(userlistItem.html).find('.selected-user').length !== 0)
				userlistItem.html = $(userlistItem.html).find('.selected-user').removeClass('selected-user').parent().parent().parent()[0].outerHTML;
		}
	}
	else {
		docLayer._followEditor = false;
		docLayer._followThis = -1;
	}
	$('#tb_actionbar_item_userlist').w2overlay('');
}

global.editorUpdate = editorUpdate;

function selectUser(viewId) {
	var userlistItem = w2ui['actionbar'].get('userlist');
	if (userlistItem === null) {
		return;
	}

	userlistItem.html = $(userlistItem.html).find('#user-' + viewId).addClass('selected-user').parent().parent().parent()[0].outerHTML;
	$('#tb_actionbar_item_userlist').w2overlay('');
}

function deselectUser(e) {
	var userlistItem = w2ui['actionbar'].get('userlist');
	if (userlistItem === null) {
		return;
	}

	userlistItem.html = $(userlistItem.html).find('#user-' + e.viewId).removeClass('selected-user').parent().parent().parent()[0].outerHTML;
}

function getUserItem(viewId, userName, extraInfo, color) {
	var html = '<tr class="useritem" id="user-' + viewId + '" onclick="onUseritemClicked(event)">' +
		     '<td class=usercolor>';
	if (extraInfo !== undefined && extraInfo.avatar !== undefined) {
		html += '<img class="avatar-img" src="' + extraInfo.avatar + '" style="border-color: ' + color  + ';" />';
	} else {
		html += '<div class="user-info" style="background-color: ' + color  + ';" />';
	}

	// TODO: Add mail and other links as sub-menu.
	html += '</td>' +
		     '<td class="username loleaflet-font" >' + userName + '</td>' +
	    '</tr>';

	return html;
}

function updateUserListCount() {
	var userlistItem = w2ui['actionbar'].get('userlist');
	if (userlistItem === null) {
		return;
	}

	var count = $(userlistItem.html).find('#userlist_table tbody tr').length;
	if (count > 1) {
		userlistItem.text = nUsers.replace('%n', count);
	} else if (count === 1) {
		userlistItem.text = oneUser;
	} else {
		userlistItem.text = noUser;
	}

	var zoomlevel = $('#zoomlevel').html();
	w2ui['actionbar'].refresh();
	$('#zoomlevel').html(zoomlevel);

	if (count > 1) {
		$('#tb_actionbar_item_userlist').show();
		$('#tb_actionbar_item_userlistbreak').show();
	} else {
		$('#tb_actionbar_item_userlist').hide();
		$('#tb_actionbar_item_userlistbreak').hide();
	}
}

function escapeHtml(input) {
	return $('<div>').text(input).html();
}

function onAddView(e) {
	var username = escapeHtml(e.username);
	$('#tb_actionbar_item_userlist')
		.w2overlay({
			class: 'loleaflet-font',
			html: userJoinedPopupMessage.replace('%user', username),
			style: 'padding: 5px'
		});
	clearTimeout(userPopupTimeout);
	userPopupTimeout = setTimeout(function() {
		$('#tb_actionbar_item_userlist').w2overlay('');
		clearTimeout(userPopupTimeout);
		userPopupTimeout = null;
	}, 3000);

	var color = L.LOUtil.rgbToHex(map.getViewColor(e.viewId));
	if (e.viewId === map._docLayer._viewId) {
		username = _('You');
		color = '#000';
	}

	// Mention readonly sessions in userlist
	if (e.readonly) {
		username += ' (' +  _('Readonly') + ')';
	}

	var userlistItem = w2ui['actionbar'].get('userlist');
	if (userlistItem !== null) {
		var newhtml = $(userlistItem.html).find('#userlist_table tbody').append(getUserItem(e.viewId, username, e.extraInfo, color)).parent().parent()[0].outerHTML;
		userlistItem.html = newhtml;
		updateUserListCount();
	}
}

function onRemoveView(e) {
	$('#tb_actionbar_item_userlist')
		.w2overlay({
			class: 'loleaflet-font',
			html: userLeftPopupMessage.replace('%user', e.username),
			style: 'padding: 5px'
		});
	clearTimeout(userPopupTimeout);
	userPopupTimeout = setTimeout(function() {
		$('#tb_actionbar_item_userlist').w2overlay('');
		clearTimeout(userPopupTimeout);
		userPopupTimeout = null;
	}, 3000);

	if (e.viewId === map._docLayer._followThis) {
		map._docLayer._followThis = -1;
		map._docLayer._followUser = false;
	}

	var userlistItem = w2ui['actionbar'].get('userlist');
	if (userlistItem !== null) {
		userlistItem.html = $(userlistItem.html).find('#user-' + e.viewId).remove().end()[0].outerHTML;
		updateUserListCount();
	}
}

$(window).resize(function() {
	resizeToolbar();
});

$(document).ready(function() {
	if (!closebutton) {
		$('#closebuttonwrapper').hide();
	} else if (closebutton && !L.Browser.mobile) {
		$('.closebuttonimage').show();
	}

	$('#closebutton').click(function() {
		map.fire('postMessage', {msgId: 'close', args: {EverModified: map._everModified, Deprecated: true}});
		map.fire('postMessage', {msgId: 'UI_Close', args: {EverModified: map._everModified}});
		map.remove();
	});

	// Attach insert file action
	$('#insertgraphic').on('change', onInsertFile);
});

function setupToolbar(e) {
	map = e;

	createToolbar();

	map.on('updateEditorName', function(e) {
		$('#currently-msg').show();
		$('#current-editor').text(e.username);
	});

	map.on('keydown', function (e) {
		if (e.originalEvent.ctrlKey && !e.originalEvent.altKey &&
		   (e.originalEvent.key === 'f' || e.originalEvent.key === 'F')) {
			var entry = L.DomUtil.get('search-input');
			entry.focus();
			entry.select();
			e.originalEvent.preventDefault();
		}
	});

	map.on('hyperlinkclicked', function (e) {
		window.open(e.url, '_blank');
	});

	map.on('cellformula', function (e) {
		if (document.activeElement !== L.DomUtil.get('formulaInput')) {
			// if the user is not editing the formula bar
			L.DomUtil.get('formulaInput').value = e.formula;
		}
	});

	map.on('zoomend', function () {
		var zoomPercent = 100;
		var zoomSelected = null;
		switch (map.getZoom()) {
		case 6:  zoomPercent =  50; zoomSelected = 'zoom50'; break;
		case 7:  zoomPercent =  60; zoomSelected = 'zoom60'; break;
		case 8:  zoomPercent =  70; zoomSelected = 'zoom70'; break;
		case 9:  zoomPercent =  85; zoomSelected = 'zoom85'; break;
		case 10: zoomPercent = 100; zoomSelected = 'zoom100'; break;
		case 11: zoomPercent = 120; zoomSelected = 'zoom120'; break;
		case 12: zoomPercent = 150; zoomSelected = 'zoom150'; break;
		case 13: zoomPercent = 175; zoomSelected = 'zoom175'; break;
		case 14: zoomPercent = 200; zoomSelected = 'zoom200'; break;
		default:
			var zoomRatio = map.getZoomScale(map.getZoom(), map.options.zoom);
			zoomPercent = Math.round(zoomRatio * 100) + '%';
			break;
		}
		$('#zoomlevel').html(zoomPercent);
		w2ui['editbar'].set('zoom', {text: zoomPercent, selected: zoomSelected});
	});

	map.on('celladdress', function (e) {
		if (document.activeElement !== L.DomUtil.get('addressInput')) {
			// if the user is not editing the address field
			L.DomUtil.get('addressInput').value = e.address;
		}
	});

	map.on('search', function (e) {
		var searchInput = L.DomUtil.get('search-input');
		var toolbar = w2ui['actionbar'];
		if (e.count === 0) {
			toolbar.disable('searchprev');
			toolbar.disable('searchnext');
			toolbar.hide('cancelsearch');
			L.DomUtil.addClass(searchInput, 'search-not-found');
			$('#findthis').addClass('search-not-found');
			map.resetSelection();
			setTimeout(function () {
				$('#findthis').removeClass('search-not-found');
				L.DomUtil.removeClass(searchInput, 'search-not-found');
			}, 500);
		}
	});

	map.on('updatetoolbarcommandvalues', function() {
		w2ui['editbar'].refresh();
	});

	map.on('showbusy', function(e) {
		w2utils.lock(w2ui['actionbar'].box, e.label, true);
	});

	map.on('hidebusy', function() {
		// If locked, unlock
		if (w2ui['actionbar'].box.firstChild.className === 'w2ui-lock') {
			w2utils.unlock(w2ui['actionbar'].box);
		}
	});

	map.on('doclayerinit', onDocLayerInit);
	map.on('wopiprops', onWopiProps);
	map.on('updatepermission', onUpdatePermission);
	map.on('commandresult', onCommandResult);
	map.on('updateparts pagenumberchanged', onUpdateParts);
	map.on('commandstatechanged', onCommandStateChanged);
}

global.setupToolbar = setupToolbar;

}(window));
