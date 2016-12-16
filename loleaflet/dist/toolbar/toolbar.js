/*
 * LibreOffice Online toolbar
 */

/* global $ map closebutton w2ui w2utils vex _ */

function onDelete(e) {
	if (e !== false) {
		map.deletePage();
	}
}

function resizeToolbar() {
	var hasMoreItems = false;
	var toolbarUp = w2ui['toolbar-up'];
	var toolbarUpMore = w2ui['toolbar-up-more'];
	// move items from toolbar-up-more -> toolbar-up
	while ($('#toolbar-up')[0].scrollWidth <= $(window).width()) {
		var item = toolbarUpMore.items[0];
		if (!item) {
			break;
		}
		toolbarUpMore.items.shift();
		toolbarUp.insert('right', item);
	}

	// move items from toolbar-up -> toolbar-up-more
	while ($('#toolbar-up')[0].scrollWidth > Math.max($(window).width(), parseInt($('body').css('min-width')))) {
		var itemId = toolbarUp.items[toolbarUp.items.length - 4].id;
		if (itemId === 'resizebreak') {
			return;
		}
		item = toolbarUp.get(itemId);
		toolbarUp.remove(itemId);
		toolbarUpMore.insert(toolbarUpMore.items[0], item);
		hasMoreItems = true;
	}

	if (hasMoreItems) {
		w2ui['toolbar-up'].show('more');
	}
	else {
		w2ui['toolbar-up'].hide('more');
	}

	// resize toolbar-up-more
	var lastItem = $('#toolbar-up-more>table>tbody>tr>td[valign="middle"]').last();
	if (lastItem.length) {
		$('#toolbar-up-more').width($(lastItem).position().left + $(lastItem).width());
		w2ui['toolbar-up-more'].render();
	} else {
		$('#toolbar-up-more').hide();
		var toolbar = w2ui['toolbar-up'];
		toolbar.uncheck('more');
	}
}

function _cancelSearch() {
	var toolbar = w2ui['toolbar-down'];
	map.resetSelection();
	toolbar.hide('cancelsearch');
	toolbar.disable('searchprev');
	toolbar.disable('searchnext');
	L.DomUtil.get('search-input').value = '';
	map.focus();
}

function onClick(id, item, subItem) {
	if (w2ui['toolbar-up'].get(id) !== null) {
		var toolbar = w2ui['toolbar-up'];
		var item = toolbar.get(id);
	}
	else if (w2ui.formulabar.get(id) !== null) {
		toolbar = w2ui.formulabar;
		item = toolbar.get(id);
	}
	else if (w2ui['toolbar-down'].get(id) !== null) {
		toolbar = w2ui['toolbar-down'];
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
	else if (w2ui['toolbar-up-more'].get(id) !== null) {
		toolbar = w2ui['toolbar-up-more'];
		item = toolbar.get(id);
	}
	else if (item && subItem)
	{
		var command = {
			'StatusBarFunc': {
				type: 'unsigned short',
				value: subItem.func
			}
		};
		map.sendUnoCommand('.uno:StatusBarFunc', command);
	}
	else {
		throw new Error('unknown id: ' + id);
	}
	var docLayer = map._docLayer;
	map.focus();
	if (item.disabled) {
		return;
	}
	if (item.uno) {
		if (item.unosheet && map.getDocType() === 'spreadsheet') {
			map.toggleCommandState(item.unosheet);
		}
		else {
			map.toggleCommandState(item.uno);
		}
	}
	else if (id === 'repair') {
		map._socket.sendMessage('commandvalues command=.uno:DocumentRepair');
	}
	else if (id === 'zoomin' && map.getZoom() < map.getMaxZoom()) {
		map.zoomIn(1);
	}
	else if (id === 'zoomout' && map.getZoom() > map.getMinZoom()) {
		map.zoomOut(1);
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
	else if (id === 'firstrecord') {
		$('#spreadsheet-tab-scroll').scrollLeft(0);
	}
	// TODO: We should get visible tab's width instead of 60px
	else if (id === 'nextrecord') {
		$('#spreadsheet-tab-scroll').scrollLeft($('#spreadsheet-tab-scroll').scrollLeft() + 60);
	}
	else if (id === 'prevrecord') {
		$('#spreadsheet-tab-scroll').scrollLeft($('#spreadsheet-tab-scroll').scrollLeft() - 60);
	}
	else if (id === 'lastrecord') {
		$('#spreadsheet-tab-scroll').scrollLeft($('#spreadsheet-tab-scroll').prop('scrollWidth'));
	}
	else if (id.startsWith('menu:wrap:wrap-'))
	{
		var wrapType = id.substring('menu:wrap:wrap-'.length);
		map.toggleCommandState(wrapType);
	}
	else if (id === 'insertgraphic') {
		L.DomUtil.get('insertgraphic').click();
	}
	else if (id === 'inserttable') {
		$('#inserttable-popup').toggle();
	}
	else if (id === 'fontcolor') {
		// absolutely no idea why, but without the timeout, the popup is
		// closed as soon as it is opend
		setTimeout(function () {
			$('#fontColorPicker').colorpicker('showPalette');
			$('.ui-widget-content').addClass('loleaflet-font');
		}, 0);
	}
	else if (id === 'backcolor') {
		// absolutely no idea why, but without the timeout, the popup is
		// closed as soon as it is opend
		setTimeout(function () {
			$('#backColorPicker').colorpicker('showPalette');
			$('.ui-widget-content').addClass('loleaflet-font');
		}, 0);
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
	else if (id === 'more') {
		$('#toolbar-up-more').toggle();
		if ($('#toolbar-up-more').is(':visible')) {
			toolbar.check('more');
		}
		else {
			toolbar.uncheck('more');
		}
		w2ui['toolbar-up-more'].render();
		resizeToolbar();
	}
	else if (id === 'close') {
		map.fire('postMessage', {msgId: 'UI_Close'});
		map.remove();
	}
	else if (id === 'specialcharacter') {
		var fontList = $('.fonts-select option');
		var selectedIndex = $('.fonts-select').prop('selectedIndex');
		map._docLayer._onSpecialChar(fontList, selectedIndex);
	}
}

function insertTable() {
	var rows = 10;
	var cols = 10;
	var $grid = $('.inserttable-grid');
	var $popup = $('#inserttable-popup');
	var $status = $('#inserttable-status');

	// Return if already initialized
	if ($grid.children().length) {
		return;
	}

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
			$popup.toggle();
			$('.col').removeClass('bright');
			$status.html('<br/>');
			var msg = 'uno .uno:InsertTable {' +
				' "Columns": { "type": "long","value": '
				+ col +
				' }, "Rows": { "type": "long","value": '
				+ row + ' }}';
			map._socket.sendMessage(msg);
			// refocus map due popup
			map.focus();
		}
	}, '.col');

	// close dialog on mouseleave
	$popup.mouseleave(function() {
		$(this).hide();
		$('.col').removeClass('bright');
		$status.html('<br/>');
	});
}

function onColorPick(e, color) {
	if (map.getPermission() !== 'edit' || color === undefined) {
		return;
	}
	// transform from #FFFFFF to an Int
	color = parseInt(color.replace('#', ''), 16);
	var command = {};
	var fontcolor, backcolor;
	if (e.target.id === 'fontColorPicker') {
		fontcolor = {'text': 'FontColor',
					 'spreadsheet': 'Color',
					 'presentation': 'Color'}[map.getDocType()];
		command[fontcolor] = {};
		command[fontcolor].type = 'long';
		command[fontcolor].value = color;
		var uno = '.uno:' + fontcolor;
	}
	else if (e.target.id === 'backColorPicker') {
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

// This object is used to store enabled/disabled state of each and every eligible item
// (i.e the one having UNO command) of toolbar-up. When the permission is changed to/from
// edit/view mode, state from this object is read and then applied on corresponding buttons
var formatButtons = {};

$(function () {
	$('#toolbar-up-more').w2toolbar({
		name: 'toolbar-up-more',
		items: [
		],
		onClick: function (e) {
			onClick(e.target);
		}
	});

	$('#toolbar-up').w2toolbar({
		name: 'toolbar-up',
		items: [
			{type: 'html', id: 'left'},
			{type: 'menu', id: 'writer:menu:wrap', caption: _('Textwrap'), items: [
				{ text: _('No wrap'), id: 'wrap-WrapOff' },
				{ text: _('Page wrap'), id: 'wrap-WrapOn' },
				{ text: _('Wrap anchor only'), id: 'wrap-WrapAnchorOnly' },
				{ text: _('Ideal wrap'), id: 'wrap-WrapIdeal' },
				{ text: _('Left wrap'), id: 'wrap-WrapLeft' },
				{ text: _('Right wrap'), id: 'wrap-WrapRight' },
				{ text: _('Wrap through'), id: 'wrap-WrapThrough' }
			]},
			{type: 'button',  id: 'save', img: 'save', hint: _('Save'), uno: 'Save'},
			{type: 'break'},
			{type: 'button',  id: 'undo',  img: 'undo', hint: _('Undo'), uno: 'Undo'},
			{type: 'button',  id: 'redo',  img: 'redo', hint: _('Redo'), uno: 'Redo'},
			{type: 'button',  id: 'repair', img: 'repair', hint: _('Document repair')},
			{type: 'break'},
			{type: 'html',   id: 'styles', html: '<select class="styles-select"></select>'},
			{type: 'html',   id: 'fonts', html: '<select class="fonts-select"></select>'},
			{type: 'html',   id: 'fontsizes', html: '<select class="fontsizes-select"></select>'},
			{type: 'break', id: 'resizebreak'},
			{type: 'button',  id: 'bold',  img: 'bold', hint: _('Bold'), uno: 'Bold'},
			{type: 'button',  id: 'italic', img: 'italic', hint: _('Italic'), uno: 'Italic'},
			{type: 'button',  id: 'underline',  img: 'underline', hint: _('Underline'), uno: 'Underline'},
			{type: 'button',  id: 'strikeout', img: 'strikeout', hint: _('Strikeout'), uno: 'Strikeout'},
			{type: 'break'},
			{type: 'button',  id: 'insertfootnote', img: 'insertfootnote', hint: _('Insert Footnote'), uno: 'InsertFootnote' },
			{type: 'break' },
			{type: 'html',  id: 'fontcolor-html', html: '<input id="fontColorPicker" style="display:none;">'},
			{type: 'button',  id: 'fontcolor', img: 'color', hint: _('Font color')},
			{type: 'html',  id: 'backcolor-html', html: '<input id="backColorPicker" style="display:none;">'},
			{type: 'button',  id: 'backcolor', img: 'backcolor', hint: _('Highlighting')},
			{type: 'break'},
			{type: 'button',  id: 'alignleft',  img: 'alignleft', hint: _('Align left'), uno: 'LeftPara', unosheet: 'HorizontalAlignment {"HorizontalAlignment":{"type":"unsigned short", "value":"1"}}'},
			{type: 'button',  id: 'alignhorizontal',  img: 'alignhorizontal', hint: _('Center horizontally'), uno: 'CenterPara', unosheet: 'HorizontalAlignment {"HorizontalAlignment":{"type":"unsigned short", "value":"2"}}'},
			{type: 'button',  id: 'alignright',  img: 'alignright', hint: _('Align right'), uno: 'RightPara', unosheet: 'HorizontalAlignment {"HorizontalAlignment":{"type":"unsigned short", "value":"3"}}'},
			{type: 'button',  id: 'alignblock',  img: 'alignblock', hint: _('Justified'), uno: 'JustifyPara', unosheet: 'HorizontalAlignment {"HorizontalAlignment":{"type":"unsigned short", "value":"4"}}'},
			{type: 'button',  id: 'wraptext',  img: 'wraptext', hint: _('Wrap Text'), uno: 'WrapText'},
			{type: 'button',  id: 'togglemergecells',  img: 'togglemergecells', hint: _('Merge and Center Cells'), uno: 'ToggleMergeCells'},
			{type: 'break',   id: 'break-toggle'},
			{type: 'button',  id: 'numberformatcurrency',  img: 'numberformatcurrency', hint: _('Format as Currency'), uno: 'NumberFormatCurrency'},
			{type: 'button',  id: 'numberformatpercent',  img: 'numberformatpercent', hint: _('Format as Percent'), uno: 'NumberFormatPercent'},
			{type: 'button',  id: 'numberformatdecimal',  img: 'numberformatdecimal', hint: _('Format as Number'), uno: 'NumberFormatDecimal'},
			{type: 'button',  id: 'numberformatdate',  img: 'numberformatdate', hint: _('Format as Date'), uno: 'NumberFormatDate'},
			{type: 'button',  id: 'numberformatincdecimals',  img: 'numberformatincdecimals', hint: _('Add Decimal Place'), uno: 'NumberFormatIncDecimals'},
			{type: 'button',  id: 'numberformatdecdecimals',  img: 'numberformatdecdecimals', hint: _('Delete Decimal Place'), uno: 'NumberFormatDecDecimals'},
			{type: 'break',   id: 'break-number'},
			{type: 'button',  id: 'sortascending',  img: 'sortascending', hint: _('Sort Ascending'), uno: 'SortAscending'},
			{type: 'button',  id: 'sortdescending',  img: 'sortdescending', hint: _('Sort Descending'), uno: 'SortDescending'},
			{type: 'break',   id: 'break-align'},
			{type: 'button',  id: 'defaultbullet',  img: 'bullet', hint: _('Bullets on/off'), uno: 'DefaultBullet'},
			{type: 'button',  id: 'defaultnumbering',  img: 'numbering', hint: _('Numbering on/off'), uno: 'DefaultNumbering'},
			{type: 'break',   id: 'break-numbering'},
			{type: 'button',  id: 'incrementindent',  img: 'incrementindent', hint: _('Increase indent'), uno: 'IncrementIndent'},
			{type: 'button',  id: 'decrementindent',  img: 'decrementindent', hint: _('Decrease indent'), uno: 'DecrementIndent'},
			{type: 'break', id: 'incdecindent'},
			{type: 'html',  id: 'inserttable-html', html: '<div id="inserttable-popup" class="inserttable-pop ui-widget ui-widget-content ui-corner-all" style="position: absolute; display: none;"><div class="inserttable-grid"></div><div id="inserttable-status" class="loleaflet-font" style="padding: 5px;"><br/></div>'},
			{type: 'button',  id: 'inserttable',  img: 'inserttable', hint: _('Insert table')},
			{type: 'button',  id: 'insertannotation', img: 'annotation', hint: _('Insert comment'), uno: 'InsertAnnotation'},
			{type: 'button',  id: 'insertgraphic',  img: 'insertgraphic', hint: _('Insert graphic')},
			{type: 'button',  id: 'specialcharacter', img: 'specialcharacter', hint: _('Special Character')},
			{type: 'html', id: 'right'},
			{type: 'button',  id: 'more', img: 'more', hint: _('More')},
			{type: 'button',  id: 'close',  img: 'closedoc', hint: _('Close document'), hidden: true}
		],
		onClick: function (e) {
			onClick(e.target);
		},
		onRefresh: function() {
			if (!L.DomUtil.get('fontcolorindicator')) {
				var fontColorIndicator = L.DomUtil.create('div', 'font-color-indicator', L.DomUtil.get('tb_toolbar-up_item_fontcolor'));
				fontColorIndicator.id = 'fontcolorindicator';

				$('#fontColorPicker').colorpicker({showOn:'none', hideButton:true});
				$('#fontColorPicker').on('change.color', onColorPick);
			}

			if (!L.DomUtil.get('backcolorindicator')) {
				var backColorIndicator = L.DomUtil.create('div', 'back-color-indicator', L.DomUtil.get('tb_toolbar-up_item_backcolor'));
				backColorIndicator.id = 'backcolorindicator';

				$('#backColorPicker').colorpicker({showOn:'none', hideButton:true});
				$('#backColorPicker').on('change.color', onColorPick);
			}

			if (map.getDocType() === 'presentation') {
				// Fill the style select box if not yet filled
				if ($('.styles-select')[0] && $('.styles-select')[0].length === 0) {
					var data = [''];
					// Inserts a separator element
					data = data.concat({text: '\u2500\u2500\u2500\u2500\u2500\u2500', disabled: true});

					L.Styles.impressLayout.forEach(function(layout) {
						data = data.concat({id: layout.id, text: _(layout.text)});
					}, this);

					$('.styles-select').select2({
						data: data,
						placeholder: _('Layout')
					});
					$('.styles-select').on('select2:select', onStyleSelect);
				}
			}

			// Intialize the formatButtons object
			if (Object.keys(formatButtons).length === 0) {
				for (var itemIdx in w2ui['toolbar-up'].items) {
					if (w2ui['toolbar-up'].items[itemIdx].uno) {
						formatButtons[w2ui['toolbar-up'].items[itemIdx].id] = true;
					}
				}
			}

			insertTable();
		}
	});

	$('#formulabar').w2toolbar({
		name: 'formulabar',
		items: [
			{type: 'html',  id: 'left'},
			{type: 'button',  id: 'sum',  img: 'autosum', hint: _('Sum')},
			{type: 'button',  id: 'function',  img: 'equal', hint: _('Function')},
			{type: 'button', hidden: true, id: 'cancelformula',  img: 'cancel', hint: _('Cancel')},
			{type: 'button', hidden: true, id: 'acceptformula',  img: 'accepttrackedchanges', hint: _('Accept')},
			{type: 'html', id: 'formula', html: '<input id="formulaInput" onkeyup="onFormulaInput(event)"' +
			 'onblur="onFormulaBarBlur()" onfocus="onFormulaBarFocus()" type=text>'}
		],
		onClick: function (e) {
			onClick(e.target);
		}
	});
	$('#spreadsheet-toolbar').w2toolbar({
		name: 'spreadsheet-toolbar',
		items: [
			{type: 'button',  id: 'firstrecord',  img: 'firstrecord', hidden: true, hint: _('First sheet')},
			{type: 'button',  id: 'prevrecord',  img: 'prevrecord', hidden: true, hint: _('Previous sheet')},
			{type: 'button',  id: 'nextrecord',  img: 'nextrecord', hidden: true, hint: _('Next sheet')},
			{type: 'button',  id: 'lastrecord',  img: 'lastrecord', hidden: true, hint: _('Last sheet')}
		],
		onClick: function (e) {
			onClick(e.target);
		}
	});
	$('#presentation-toolbar').w2toolbar({
		name: 'presentation-toolbar',
		items: [
			{type: 'html',  id: 'left'},
			{type: 'button',  id: 'presentation', img: 'presentation', hidden:true, hint: _('Fullscreen presentation')},
			{type: 'break', id: 'presentationbreak', hidden:true},
			{type: 'button',  id: 'insertpage', img: 'insertpage', hidden:true, hint: _('Insert slide')},
			{type: 'button',  id: 'duplicatepage', img: 'duplicatepage', hidden:true, hint: _('Duplicate slide')},
			{type: 'button',  id: 'deletepage', img: 'deletepage', hidden:true, hint: _('Delete slide')},
			{type: 'html',  id: 'right'}
		],
		onClick: function (e) {
			onClick(e.target);
		}
	});

	$('#toolbar-down').w2toolbar({
		name: 'toolbar-down',
		items: [
			{type: 'html',  id: 'search',
			 html: '<div style="padding: 3px 10px;" class="loleaflet-font">' +
			 ' ' + _('Search:') +
			 '    <input size="10" id="search-input" oninput="onSearch(event)" onkeypress="onSearchKeyPress(event)" ' +
			 'style="padding: 3px; border-radius: 2px; border: 1px solid silver"/>' +
			 '</div>'
			},
			{type: 'button',  id: 'searchprev', img: 'prev', hint: _('Search backwards'), disabled: true},
			{type: 'button',  id: 'searchnext', img: 'next', hint: _('Search forward'), disabled: true},
			{type: 'button',  id: 'cancelsearch', img: 'cancel', hint: _('Cancel the search'), hidden: true},
			{type: 'html',  id: 'left'},
			{type: 'html',  id: 'right'},
			{type: 'html',    id: 'modifiedstatuslabel', html: '<div id="modifiedstatuslabel" class="loleaflet-font"></div>'},
			{type: 'break'},
			{type: 'drop', id: 'userlist', text: _('No users'), html: '<div id="userlist_container"><table id="userlist_table"><tbody></tbody></table></div>' },
			{type: 'break'},
			{type: 'button',  id: 'prev', img: 'prev', hint: _('Previous page')},
			{type: 'button',  id: 'next', img: 'next', hint: _('Next page')},
			{type: 'break', id: 'prevnextbreak'},
			{type: 'button',  id: 'zoomreset', img: 'zoomreset', hint: _('Reset zoom')},
			{type: 'button',  id: 'zoomout', img: 'zoomout', hint: _('Zoom out')},
			{type: 'html',    id: 'zoomlevel', html: '<div id="zoomlevel" class="loleaflet-font">100%</div>'},
			{type: 'button',  id: 'zoomin', img: 'zoomin', hint: _('Zoom in')}
		],
		onClick: function (e) {
			if (e.item.id === 'userlist') {
				return;
			}
			onClick(e.target, e.item, e.subItem);
		},
		onRefresh: function(e) {
			$('#tb_toolbar-down_item_userlist .w2ui-tb-caption').addClass('loleaflet-font');
		}
	});
});

var userJoinedPopupMessage = '<div>' + _('%user has joined') + '</div>';
var userLeftPopupMessage = '<div>' + _('%user has left') + '</div>';
var userPopupTimeout = null;

function toLocalePattern (pattern, regex, text, sub1, sub2) {
	var matches = new RegExp(regex, 'g').exec(text);
	if (matches) {
		text = pattern.toLocaleString().replace(sub1, parseInt(matches[1].replace(',','')).toLocaleString(String.locale)).replace(sub2, parseInt(matches[2].replace(',','')).toLocaleString(String.locale));
	}
	return text;
}

function selectItem(item, func)
{
	var index = -1;
	for (var it = 0; it < item.items.length; it++) {
		if (item.items[it].func === func) {
			index = it;
			break;
		}
	}

	if (index !== -1) {
		item.items[item.current].icon = '';
		item.items[index].icon = 'selected';
		item.current = index;
	}
}

function onSearch(e) {
	var toolbar = w2ui['toolbar-down'];
	map.search(L.DomUtil.get('search-input').value);
	toolbar.enable('searchprev');
	toolbar.enable('searchnext');
	toolbar.show('cancelsearch');
}

function onSearchKeyPress(e) {
	if (e.keyCode === 13) { // Enter key
		if (e.shiftKey) {
			// search backwards
			map.search(L.DomUtil.get('search-input').value, true);
		} else {
			map.search(L.DomUtil.get('search-input').value);
		}
	} else if (e.keyCode === 27) { // Escape key
		_cancelSearch();
	}
}

function onSaveAs(e) {
	if (e !== false) {
		map.saveAs(e.url, e.format, e.options);
	}
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
		placeholder: _('Size'),
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

map.on('hidebusy', function() {
	// If locked, unlock
	if (w2ui['toolbar-down'].box.firstChild.className === 'w2ui-lock') {
		w2utils.unlock(w2ui['toolbar-down'].box);
	}
});

map.on('showbusy', function(e) {
	w2utils.lock(w2ui['toolbar-down'].box, e.label, true);
});

map.on('wopiprops', function(e) {
	if (e.HideSaveOption) {
		w2ui['toolbar-up'].hide('save');
	}
});

map.on('doclayerinit', function () {
	var toolbarUp = w2ui['toolbar-up'];
	var toolbarUpMore = w2ui['toolbar-up-more'];
	var statusbar = w2ui['toolbar-down'];
	var docType = map.getDocType();

	switch (docType) {
	case 'spreadsheet':
		toolbarUp.remove('inserttable', 'styles', 'alignblock', 'bullet', 'numbering', 'break-numbering');
		toolbarUpMore.remove('inserttable', 'styles', 'alignblock', 'bullet', 'numbering', 'break-numbering');
		statusbar.disable('zoomreset', 'zoomout', 'zoomin', 'zoomlevel');
		statusbar.insert('left', [
			{type: 'break', id:'break1'},
			{type: 'html',  id: 'StatusDocPos',
				html: '<div id="StatusDocPos" class="loleaflet-font" title="'+_('Number of Sheets')+ '" style="padding: 5px 5px;">&nbsp;&nbsp;&nbsp;&nbsp;&nbsp</div>' },
			{type: 'break', id:'break2'},
			{type: 'html',  id: 'RowColSelCount',
				html: '<div id="RowColSelCount" class="loleaflet-font" title="'+_('Selected range of cells')+ '" style="padding: 5px 5px;">&nbsp;&nbsp;&nbsp;&nbsp;&nbsp</div>' },
			{type: 'break', id:'break3'},
			{type: 'html',  id: 'InsertMode',
				html: '<div id="InsertMode" class="loleaflet-font" title="'+_('Entering text mode')+ '" style="padding: 5px 5px;">&nbsp;&nbsp;&nbsp;&nbsp;&nbsp</div>' },
			{type: 'break', id:'break5'},
			{type: 'html',  id: 'StatusSelectionMode',
				html: '<div id="StatusSelectionMode" class="loleaflet-font" title="'+_('Selection Mode')+ '" style="padding: 5px 5px;">&nbsp;&nbsp;&nbsp;&nbsp;&nbsp</div>' },
			{type: 'break', id:'break8'},
			{type: 'html',  id: 'StateTableCell',
				html: '<div id="StateTableCell" class="loleaflet-font" title="'+_('Choice of functions')+ '" style="padding: 5px 5px;">&nbsp;&nbsp;&nbsp;&nbsp;&nbsp</div>' },
		        {type: 'menu', id: 'StateTableCellMenu', caption: '', current: 7, items: [
				{ func: '1', text: _('Average'), icon: ''},
				{ func: '3', text: _('CountA'), icon: ''},
				{ func: '2', text: _('Count'), icon: ''},
				{ func: '4', text: _('Maximum'), icon: ''},
				{ func: '5', text: _('Minimum'), icon: ''},
				{ func: '9', text: _('Sum'), icon: ''},
				{ func: '12', text: _('Selection count'), icon: ''},
				{ func: '16', text: _('None'), icon: 'selected'},
		]},
		]);
		break;
	case 'text':
		toolbarUp.remove('wraptext', 'togglemergecells', 'break-toggle', 'numberformatcurrency', 'numberformatpercent', 'numberformatdecimal', 'numberformatdate', 'numberformatincdecimals', 'numberformatdecdecimals', 'break-number', 'sortascending', 'sortdescending');
		toolbarUpMore.remove('wraptext', 'togglemergecells', 'break-toggle', 'numberformatcurrency', 'numberformatpercent', 'numberformatdecimal', 'numberformatdate', 'numberformatincdecimals', 'numberformatdecdecimals', 'break-number', 'sortascending', 'sortdescending');
		statusbar.insert('left', [
			{type: 'break', id:'break1'},
			{type: 'html',  id: 'StatePageNumber',
				html: '<div id="StatePageNumber" class="loleaflet-font" title="'+_('Number of Pages')+ '" style="padding: 5px 5px;">&nbsp;&nbsp;&nbsp;&nbsp;&nbsp</div>' },
			{type: 'break', id:'break2'},
			{type: 'html',  id: 'StateWordCount',
				html: '<div id="StateWordCount" class="loleaflet-font" title="'+_('Word Counter')+ '" style="padding: 5px 5px;">&nbsp;&nbsp;&nbsp;&nbsp;&nbsp</div>' },
			{type: 'break', id:'break5'},
			{type: 'html',  id: 'InsertMode',
				html: '<div id="InsertMode" class="loleaflet-font" title="'+_('Entering text mode')+ '" style="padding: 5px 5px;">&nbsp;&nbsp;&nbsp;&nbsp;&nbsp</div>' },
			{type: 'break', id:'break6'},
			{type: 'html',  id: 'SelectionMode',
				html: '<div id="StatusSelectionMode" class="loleaflet-font" title="'+_('Selection Mode')+ '" style="padding: 5px 5px;">&nbsp;&nbsp;&nbsp;&nbsp;&nbsp</div>' },
		]);
		break;
	case 'presentation':
		var presentationToolbar = w2ui['presentation-toolbar'];
		presentationToolbar.show('presentation', 'presentationbreak', 'insertpage', 'duplicatepage', 'deletepage');
		toolbarUp.remove('annotation', 'wraptext', 'togglemergecells', 'break-toggle', 'numberformatcurrency', 'numberformatpercent', 'numberformatdecimal', 'numberformatdate', 'numberformatincdecimals', 'numberformatdecdecimals', 'break-number', 'sortascending', 'sortdescending', 'specialcharacter');
		toolbarUpMore.remove('annotation', 'wraptext', 'togglemergecells', 'break-toggle', 'numberformatcurrency', 'numberformatpercent', 'numberformatdecimal', 'numberformatdate', 'numberformatincdecimals', 'numberformatdecdecimals', 'break-number', 'sortascending', 'sortdescending', 'specialcharacter');
		statusbar.insert('left', [
			{type: 'break', id:'break1'},
			{type: 'html',  id: 'PageStatus',
				html: '<div id="PageStatus" class="loleaflet-font" title="'+_('Number of Slides')+ '" style="padding: 5px 5px;">&nbsp;&nbsp;&nbsp;&nbsp;&nbsp</div>' },
		]);
		break;
	case 'drawing':
		toolbarUp.remove('annotation', 'wraptext', 'togglemergecells', 'break-toggle', 'numberformatcurrency', 'numberformatpercent', 'numberformatdecimal', 'numberformatdate', 'numberformatincdecimals', 'numberformatdecdecimals', 'break-number', 'sortascending', 'sortdescending', 'specialcharacter');
		toolbarUpMore.remove('annotation', 'wraptext', 'togglemergecells', 'break-toggle', 'numberformatcurrency', 'numberformatpercent', 'numberformatdecimal', 'numberformatdate', 'numberformatincdecimals', 'numberformatdecdecimals', 'break-number', 'sortascending', 'sortdescending', 'specialcharacter');
		break;
	}
	toolbarUp.refresh();
	toolbarUpMore.refresh();
	statusbar.refresh();
	resizeToolbar();
});

map.on('commandstatechanged', function (e) {
	var toolbar = w2ui['toolbar-up'];
	var toolbarUpMore = w2ui['toolbar-up-more'];
	var statusbar = w2ui['toolbar-down'];
	var commandName = e.commandName;
	var state = e.state;
	var found = false;
	var value, color, div;
	var matches;
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
			color = '#' + '0'.repeat(6 - color.length) + color;
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
			color = '#' + '0'.repeat(6 - color.length) + color;
		}
		div = L.DomUtil.get('backcolorindicator');
		if (div) {
			L.DomUtil.setStyle(div, 'background', color);
		}
	}
	else if (commandName === '.uno:ModifiedStatus') {
		var modifiedStatus = e.state === 'true';
		if (modifiedStatus) {
			$('#modifiedstatuslabel').html('');
		}
		else {
			$('#modifiedstatuslabel').html(_('Document saved'));
		}
	}
	else if (commandName === '.uno:StatusDocPos') {
		state = toLocalePattern('Sheet %1 of %2', 'Sheet (\\d+) of (\\d+)', state, '%1', '%2');
		$('#StatusDocPos').html(state ? state : '&nbsp;&nbsp;&nbsp;&nbsp;&nbsp');
	}
	else if (commandName === '.uno:RowColSelCount') {
		state = toLocalePattern('$1 rows, $2 columns selected', '(\\d+) rows, (\\d+) columns selected', state, '$1', '$2');
		$('#RowColSelCount').html(state ? state : '&nbsp;&nbsp;&nbsp;&nbsp;&nbsp');
	}
	else if (commandName === '.uno:InsertMode') {
		$('#InsertMode').html(state ? L.Styles.insertMode[state].toLocaleString() : '&nbsp;&nbsp;&nbsp;&nbsp;&nbsp');
	}
	else if (commandName === '.uno:StatusSelectionMode' ||
		 commandName === '.uno:SelectionMode') {
		$('#StatusSelectionMode').html(state ? L.Styles.selectionMode[state].toLocaleString() : '&nbsp;&nbsp;&nbsp;&nbsp;&nbsp');
	}
	else if (commandName == '.uno:StateTableCell') {
		$('#StateTableCell').html(state ? state : '&nbsp;&nbsp;&nbsp;&nbsp;&nbsp');
	}
	else if (commandName === '.uno:StatusBarFunc') {
		if (state) {
			selectItem(statusbar.get('StateTableCellMenu'), state);
		}
	}
	else if (commandName === '.uno:StatePageNumber') {
		state = toLocalePattern('Page %1 of %2', 'Page (\\d+) of (\\d+)', state, '%1', '%2');
		$('#StatePageNumber').html(state ? state : '&nbsp;&nbsp;&nbsp;&nbsp;&nbsp');
	}
	else if (commandName === '.uno:StateWordCount') {
		state = toLocalePattern('%1 words, %2 characters', '([\\d,]+) words, ([\\d,]+) characters', state, '%1', '%2');
		$('#StateWordCount').html(state ? state : '&nbsp;&nbsp;&nbsp;&nbsp;&nbsp');
	}
	else if (commandName === '.uno:PageStatus') {
		state = toLocalePattern('Slide %1 of %2', 'Slide (\\d+) of (\\d+)', state, '%1', '%2');
		$('#PageStatus').html(state ? state : '&nbsp;&nbsp;&nbsp;&nbsp;&nbsp');
	}

	var id = commandName.toLowerCase().substr(5);
	if (typeof formatButtons[id] !== 'undefined') {
		if (state === 'true') {
			toolbar.enable(id);
			toolbar.check(id);
			toolbarUpMore.check(id);
		}
		else if (state === 'false') {
			toolbar.enable(id);
			toolbar.uncheck(id);
			toolbarUpMore.uncheck(id);
		}
		// only store the state for now;
		// buttons with stored state === enabled will
		// be enabled later (if we are in editmode)
		// If we are in viewmode, these store states will be used
		// when we get the edit access
		else if (state === 'enabled') {
			formatButtons[id] = true;
		}
		else if (state === 'disabled') {
			formatButtons[id] = false;
		}

		// Change the toolbar button states immediately if we are in editmode
		if (map._permission === 'edit' && (state === 'enabled' || state === 'disabled')) {
			// in case some buttons are in toolbar-up-more, find
			// them and en/dis-able them.
			if (formatButtons[id]) {
				toolbar.enable(id);
				toolbarUpMore.enable(id);
			} else {
				toolbar.uncheck(id);
				toolbar.disable(id);
				toolbarUpMore.disable(id);
			}
		}
	}
});

map.on('search', function (e) {
	var searchInput = L.DomUtil.get('search-input');
	var toolbar = w2ui['toolbar-down'];
	if (e.count === 0) {
		toolbar.disable('searchprev');
		toolbar.disable('searchnext');
		toolbar.hide('cancelsearch');
		L.DomUtil.addClass(searchInput, 'search-not-found');
		setTimeout(function () {
			L.DomUtil.removeClass(searchInput, 'search-not-found');
		}, 500);
	}
});

map.on('updatetoolbarcommandvalues', function (e) {
	// we need an empty option for the place holder to work
	var data = [];
	var styles = [];
	var topStyles = [];
	if (e.commandName === '.uno:StyleApply') {
		var commands = e.commandValues.Commands;
		if (commands && commands.length > 0) {
			// Inserts a separator element
			data = data.concat({text: '\u2500\u2500\u2500\u2500\u2500\u2500', disabled: true});

			commands.forEach(function (command) {
				data = data.concat({id: command.id, text: L.Styles.styleMappings[command.text].toLocaleString()});
			}, this);
		}

		if (map.getDocType() === 'text') {
			styles = e.commandValues.ParagraphStyles.slice(7, 19);
			topStyles = e.commandValues.ParagraphStyles.slice(0, 7);
		}
		else if (map.getDocType() === 'spreadsheet') {
			styles = e.commandValues.CellStyles;
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
		$('.styles-select').on('select2:select', onStyleSelect);
	}
	else if (e.commandName === '.uno:CharFontName') {
		// Old browsers like IE11 et al don't like Object.keys with
		// empty arguments
		if (typeof e.commandValues === 'object') {
			data = data.concat(Object.keys(e.commandValues));
		}
		$('.fonts-select').select2({
			data: data,
			placeholder: _('Font')
		});
		$('.fonts-select').on('select2:select', onFontSelect);

		$('.fontsizes-select').select2({
			placeholder: _('Size'),
			data: []
		});
		$('.fontsizes-select').on('select2:select', onFontSizeSelect);
	}
});

map.on('updateparts pagenumberchanged', function (e) {
	if (e.docType === 'text') {
		var current = e.currentPage;
		var count = e.pages;
	}
	else {
		current = e.selectedPart;
		count = e.parts;
	}

	var toolbar = w2ui['toolbar-down'];
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

	if (e.docType === 'spreadsheet') {
		toolbar.hide('prev');
		toolbar.hide('next');
		toolbar.hide('prevnextbreak');
	}
	else {
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

	toolbar = w2ui['toolbar-up'];
	if (e.docType !== 'text' && e.docType !== 'spreadsheet') {
		toolbar.hide('incrementindent');
		toolbar.hide('decrementindent');
		toolbar.hide('incdecindent');
	}

	toolbar = w2ui['spreadsheet-toolbar'];
	if (e.docType === 'spreadsheet') {
		toolbar.show('firstrecord');
		toolbar.show('nextrecord');
		toolbar.show('prevrecord');
		toolbar.show('lastrecord');
	}
});

map.on('commandresult', function (e) {
	var commandName = e.commandName;

	if (commandName === '.uno:Save' && e.success === true) {
		// owncloud integration
		if (typeof window.parent.documentsMain !== 'undefined') {
			window.parent.documentsMain.saveDocumentBack();
		}
	}
	else if ((commandName === '.uno:Undo' || commandName === '.uno:Redo') &&
		e.success === true && e.result.value && e.result.value === '130') { /*UNDO_CONFLICT*/
		$('#tb_toolbar-up_item_repair').w2overlay({ html: '<div style="padding: 10px; line-height: 150%">' +
			_('Conflict Undo/Redo with multiple users. Please use document repair to resolve') + '</div>'});
	}
});

map.on('cellformula', function (e) {
	if (document.activeElement !== L.DomUtil.get('formulaInput')) {
		// if the user is not editing the formula bar
		L.DomUtil.get('formulaInput').value = e.formula;
	}
});

map.on('zoomend', function () {
	var zoomRatio = map.getZoomScale(map.getZoom(), map.options.zoom);
	var zoomPercent = Math.round(zoomRatio * 100);
	$('#zoomlevel').html(zoomPercent + '%');
});

map.on('hyperlinkclicked', function (e) {
	window.open(e.url, '_blank');
});

map.on('updatepermission', function (e) {
	var toolbar = w2ui['toolbar-up'];
	var toolbarUpMore = w2ui['toolbar-up-more'];
	// {En,Dis}able toolbar buttons
	for (var id in formatButtons) {
		if (e.perm === 'edit' && formatButtons[id]) {
			// restore the state from stored object (formatButtons)
			toolbar.enable(id);
			// some might be hidden in toolbar-up-more
			toolbarUpMore.enable(id);
		} else {
			toolbar.disable(id);
			toolbarUpMore.disable(id);
		}
	}

	var spreadsheetButtons = ['firstrecord', 'prevrecord', 'nextrecord', 'lastrecord'];
	var formulaBarButtons = ['sum', 'function'];
	var presentationButtons = ['insertpage', 'duplicatepage', 'deletepage'];
	var toolbarDownButtons = ['next', 'prev'];
	if (e.perm === 'edit') {
		// Enable list boxes
		$('.styles-select').prop('disabled', false);
		$('.fonts-select').prop('disabled', false);
		$('.fontsizes-select').prop('disabled', false);

		// Enable formula bar
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

		toolbar = w2ui['toolbar-down'];
		toolbarDownButtons.forEach(function(id) {
			toolbar.enable(id);
		});
		$('#search-input').prop('disabled', false);
	}
	else {
		// Disable list boxes
		$('.styles-select').prop('disabled', true);
		$('.fonts-select').prop('disabled', true);
		$('.fontsizes-select').prop('disabled', true);

		// Disable formula bar
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

		toolbar = w2ui['toolbar-down'];
		toolbarDownButtons.forEach(function(id) {
			toolbar.disable(id);
		});
		$('#search-input').prop('disabled', true);
	}
});

map.on('keydown', function (e) {
	if (e.originalEvent.ctrlKey && !e.originalEvent.altKey &&
	   (e.originalEvent.key === 'f' || e.originalEvent.key === 'F')) {
		L.DomUtil.get('search-input').focus();
		e.originalEvent.preventDefault();
	}
});

function onUseritemClicked(e) {
	var viewId = parseInt(e.currentTarget.id.replace('user-', ''));

	if (map._docLayer) {
		if (map.getDocType() === 'spreadsheet') {
			map._docLayer.goToCellViewCursor(viewId);
		} else if (map.getDocType() === 'text') {
			map._docLayer.goToViewCursor(viewId);
		}
	}
}

function getUserItem(viewId, userName, color) {
	var html = '<tr class="useritem" id="user-' + viewId + '" onclick="onUseritemClicked(event)">' +
	             '<td class=usercolor style="background-color: ' + color  +';"></td>' +
	             '<td class="username loleaflet-font">' + userName + '</td>' +
	    '</tr>';

	return html;
}
var nUsers = _('%n users');
function updateUserListCount() {
	var userlistItem = w2ui['toolbar-down'].get('userlist');
	var count = $(userlistItem.html).find('#userlist_table tbody tr').length;
	if (count > 1) {
		userlistItem.text = nUsers.replace('%n', count);
	} else if (count === 1) {
		userlistItem.text = _('1 user');
	} else {
		userlistItem.text = _('No users');
	}

	var zoomlevel = $('#zoomlevel').html();
	w2ui['toolbar-down'].refresh();
	$('#zoomlevel').html(zoomlevel);
}

map.on('addview', function(e) {
	$('#tb_toolbar-down_item_userlist')
		.w2overlay({
			class: 'loleaflet-font',
			html: userJoinedPopupMessage.replace('%user', e.username),
			style: 'padding: 5px'
		});
	clearTimeout(userPopupTimeout);
	userPopupTimeout = setTimeout(function() {
		$('#tb_toolbar-down_item_userlist').w2overlay('');
		clearTimeout(userPopupTimeout);
		userPopupTimeout = null;
	}, 3000);

	var username = e.username;
	var color = L.LOUtil.rgbToHex(map.getViewColor(e.viewId));
	if (e.viewId === map._docLayer._viewId) {
		username = _('You');
		color = '#000';
	}
	var userlistItem = w2ui['toolbar-down'].get('userlist');
	var newhtml = $(userlistItem.html).find('#userlist_table tbody').append(getUserItem(e.viewId, username, color)).parent().parent()[0].outerHTML;
	userlistItem.html = newhtml;
	updateUserListCount();
});

map.on('removeview', function(e) {
	$('#tb_toolbar-down_item_userlist')
		.w2overlay({
			class: 'loleaflet-font',
			html: userLeftPopupMessage.replace('%user', e.username),
			style: 'padding: 5px'
		});
	clearTimeout(userPopupTimeout);
	userPopupTimeout = setTimeout(function() {
		$('#tb_toolbar-down_item_userlist').w2overlay('');
		clearTimeout(userPopupTimeout);
		userPopupTimeout = null;
	}, 3000);

	var userlistItem = w2ui['toolbar-down'].get('userlist');
	userlistItem.html = $(userlistItem.html).find('#user-' + e.viewId).remove().end()[0].outerHTML;
	updateUserListCount();
});

$(window).resize(function() {
	resizeToolbar();
});

$(document).ready(function() {
	var toolbar = w2ui['toolbar-up'];
	if (closebutton) {
		toolbar.show('close');
	}
});
