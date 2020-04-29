/* -*- js-indent-level: 8 -*- */
/*
 * L.Control.NotebookbarBuilder
 */

/* global $ _ _UNO */
L.Control.NotebookbarBuilder = L.Control.JSDialogBuilder.extend({

	_customizeOptions: function() {
		this.options.noLabelsForUnoButtons = true;
		this.options.cssClass = 'notebookbar';
	},

	_overrideHandlers: function() {
		this._controlHandlers['combobox'] = this._comboboxControl;
		this._controlHandlers['listbox'] = this._comboboxControl;
		this._controlHandlers['tabcontrol'] = this._overridenTabsControlHandler;

		this._controlHandlers['pushbutton'] = function() { return false; };
		this._controlHandlers['spinfield'] = function() { return false; };

		this._toolitemHandlers['.uno:XLineColor'] = this._colorControl;
		this._toolitemHandlers['.uno:FontColor'] = this._colorControl;
		this._toolitemHandlers['.uno:BackColor'] = this._colorControl;
		this._toolitemHandlers['.uno:CharBackColor'] = this._colorControl;
		this._toolitemHandlers['.uno:BackgroundColor'] = this._colorControl;
		this._toolitemHandlers['.uno:FrameLineColor'] = this._colorControl;
		this._toolitemHandlers['.uno:Color'] = this._colorControl;
		this._toolitemHandlers['.uno:FillColor'] = this._colorControl;

		this._toolitemHandlers['.uno:InsertTable'] = this._insertTableControl;
		this._toolitemHandlers['.uno:InsertGraphic'] = this._insertGraphicControl;

		this._toolitemHandlers['.uno:SelectWidth'] = function() {};

		this._toolitemHandlers['vnd.sun.star.findbar:FocusToFindbar'] = function() {};
	},

	onCommandStateChanged: function(e) {
		var commandName = e.commandName;
		var state = e.state;

		if (commandName === '.uno:CharFontName') {
			$('#fontnamecombobox').val(state).trigger('change');
		} else if (commandName === '.uno:FontHeight') {
			$('#fontsizecombobox').val(state).trigger('change');
		} else if (commandName === '.uno:StyleApply') {
			$('#applystyle').val(state).trigger('change');
		}
	},

	_setupComboboxSelectionHandler: function(combobox, id, builder) {
		var items = builder.map['stateChangeHandler'];

		if (id === 'fontnamecombobox') {
			$(combobox).on('select2:select', function (e) {
				var font = e.target.value;
				builder.map.applyFont(font);
				builder.map.focus();
			});

			var state = items.getItemValue('.uno:CharFontName');
			$(combobox).val(state).trigger('change');
		}
		else if (id === 'fontsizecombobox') {
			$(combobox).on('select2:select', function (e) {
				builder.map.applyFontSize(e.target.value);
				builder.map.focus();
			});

			state = items.getItemValue('.uno:FontHeight');
			$(combobox).val(state).trigger('change');
		}
		else if (id === 'applystyle') {
			$(combobox).on('select2:select', function (e) {
				var style = e.target.value;
				var docType = builder.map.getDocType();

				if (style.startsWith('.uno:'))
					builder.map.sendUnoCommand(style);
				else if (docType === 'text')
					builder.map.applyStyle(style, 'ParagraphStyles');
				else if (docType === 'spreadsheet')
					builder.map.applyStyle(style, 'CellStyles');
				else if (docType === 'presentation' || docType === 'drawing')
					builder.map.applyLayout(style);

				builder.map.focus();
			});

			state = items.getItemValue('.uno:StyleApply');
			$(combobox).val(state).trigger('change');
		}
	},

	_comboboxControl: function(parentContainer, data, builder) {
		if (!data.entries || data.entries.length === 0)
			return false;

		var select = L.DomUtil.createWithId('select', data.id, parentContainer);
		$(select).addClass(builder.options.cssClass);

		$(select).select2({
			data: data.entries.sort(function (a, b) {return a.localeCompare(b);}),
			placeholder: _(builder._cleanText(data.text))
		});

		builder._setupComboboxSelectionHandler(select, data.id, builder);

		return false;
	},

	_overridenTabsControlHandler: function(parentContainer, data, builder) {
		data.tabs = builder.wizard.getTabs();
		return builder._tabsControlHandler(parentContainer, data, builder);
	},

	_colorControl: function(parentContainer, data, builder) {
		var commandOverride = data.command === '.uno:Color';
		if (commandOverride)
			data.command = '.uno:FontColor';

		var titleOverride = builder._getTitleForControlWithId(data.id);
		if (titleOverride)
			data.text = titleOverride;

		data.id = data.id ? data.id : (data.command ? data.command.replace('.uno:', '') : undefined);

		data.text = builder._cleanText(data.text);

		if (data.command) {
			var div = builder._createIdentifiable('div', 'unotoolbutton ' + builder.options.cssClass + ' ui-content unospan', parentContainer, data);

			var id = data.command.substr('.uno:'.length);
			div.id = id;

			div.title = data.text;
			$(div).tooltip();

			var icon = builder._createIconPath(data.command);
			var buttonId = id + 'img';

			var button = L.DomUtil.create('img', 'ui-content unobutton', div);
			button.src = icon;
			button.id = buttonId;

			var valueNode =  L.DomUtil.create('div', 'selected-color', div);

			var selectedColor;

			var updateFunction = function (color) {
				selectedColor = builder._getCurrentColor(data, builder);
				valueNode.style.backgroundColor = color ? color : selectedColor;
			};

			updateFunction();

			builder.map.on('commandstatechanged', function(e) {
				if (e.commandName === data.command)
					updateFunction();
			}, this);

			var noColorControl = (data.command !== '.uno:FontColor' && data.command !== '.uno:Color');

			$(div).click(function() {
				$(div).w2color({ color: selectedColor, transparent: noColorControl }, function (color) {
					if (color != null) {
						if (color) {
							updateFunction('#' + color);
							builder._sendColorCommand(builder, data, color);
						} else {
							updateFunction('#FFFFFF');
							builder._sendColorCommand(builder, data, 'transparent');
						}
					}
				});
			});
		}

		return false;
	},

	_insertTableControl: function(parentContainer, data, builder) {
		var control = builder._unoToolButton(parentContainer, data, builder);

		$(control.container).unbind('click');
		$(control.container).click(function () {
			if (!$('.inserttable-grid').length) {
				$(control.container).w2overlay(window.getInsertTablePopupHtml());
				window.insertTable();

				$('.inserttable-grid .row .col').click(function () {
					$(control.container).w2overlay();
				});
			}
		});
	},

	_insertGraphicControl: function(parentContainer, data, builder) {
		var control = builder._unoToolButton(parentContainer, data, builder);

		$(control.container).unbind('click');
		$(control.container).click(function () {
			if (builder.map['wopi'].EnableInsertRemoteImage) {
				$(control.container).w2menu({
					items: [
						{id: 'localgraphic', text: _('Insert Local Image')},
						{id: 'remotegraphic', text: _UNO('.uno:InsertGraphic', '', true)}
					],
					onSelect: function (event) {
						if (event.item.id === 'localgraphic') {
							L.DomUtil.get('insertgraphic').click();
						} else if (event.item.id === 'remotegraphic') {
							builder.map.fire('postMessage', {msgId: 'UI_InsertGraphic'});
						}
					}
				});
			} else {
				L.DomUtil.get('insertgraphic').click();
			}
		});
	},

	build: function(parent, data, hasVerticalParent, parentHasManyChildren) {
		this._amendJSDialogData(data);

		if (hasVerticalParent === undefined) {
			parent = L.DomUtil.create('table', 'root-container ' + this.options.cssClass, parent);
			parent = L.DomUtil.create('tr', '', parent);
		}

		var containerToInsert = parent;

		for (var childIndex in data) {
			var childData = data[childIndex];
			if (!childData)
				continue;

			var childType = childData.type;
			if (childType === 'toolbox' && !childData.id)
				continue;

			if (parentHasManyChildren) {
				if (!hasVerticalParent)
					var td = L.DomUtil.create('td', '', containerToInsert);
				else {
					containerToInsert = L.DomUtil.create('tr', '', parent);
					td = L.DomUtil.create('td', '', containerToInsert);
				}
			} else {
				td = containerToInsert;
			}

			var isVertical = childData.vertical === 'true' ? true : false;

			this._parentize(childData);
			var processChildren = true;

			if ((childData.id === undefined || childData.id === '' || childData.id === null)
				&& (childType == 'checkbox' || childType == 'radiobutton')) {
				continue;
			}

			var hasManyChildren = childData.children && childData.children.length > 1;
			if (hasManyChildren) {
				var table = L.DomUtil.createWithId('table', 'table-' + childData.id, td);
				$(table).addClass(this.options.cssClass);
				var childObject = L.DomUtil.create('tr', '', table);
			} else {
				childObject = td;
			}

			var handler = this._controlHandlers[childType];
			var twoPanelsAsChildren =
			    childData.children && childData.children.length == 2
			    && childData.children[0] && childData.children[0].type == 'panel'
			    && childData.children[1] && childData.children[1].type == 'panel';

			if (twoPanelsAsChildren) {
				handler = this._controlHandlers['paneltabs'];
				processChildren = handler(childObject, childData.children, this);
			} else {
				if (handler)
					processChildren = handler(childObject, childData, this);
				else
					console.warn('Unsupported control type: \"' + childType + '\"');

				if (processChildren && childData.children != undefined)
					this.build(childObject, childData.children, isVertical, hasManyChildren);
				else if (childData.visible && (childData.visible === false || childData.visible === 'false')) {
					$('#' + childData.id).addClass('hidden-from-event');
				}
			}
		}
	}

});

L.control.notebookbarBuilder = function (options) {
	var builder = new L.Control.NotebookbarBuilder(options);
	builder._setup(options);
	builder._overrideHandlers();
	builder._customizeOptions();
	options.map.on('commandstatechanged', builder.onCommandStateChanged, builder);
	return builder;
};
