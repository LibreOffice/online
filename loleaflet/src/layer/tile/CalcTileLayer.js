/* -*- js-indent-level: 8 -*- */
/*
 * Calc tile layer is used to display a spreadsheet document
 */

/* global $ _ w2ui w2utils _UNO */
L.CalcTileLayer = L.TileLayer.extend({
	STD_EXTRA_WIDTH: 113, /* 2mm extra for optimal width,
							  * 0.1986cm with TeX points,
							  * 0.1993cm with PS points. */

	twipsToHMM: function (twips) {
		return (twips * 127 + 36) / 72;
	},

	newAnnotation: function (comment) {
		if (window.mode.isMobile() || window.mode.isTablet()) {
			var that = this;
			this.newAnnotationVex(comment, function(annotation) { that._onAnnotationSave(annotation); });
		} else {
			var annotations = this._annotations[this._selectedPart];
			var annotation;
			for (var key in annotations) {
				if (this._cellCursor.contains(annotations[key]._annotation._data.cellPos)) {
					annotation = annotations[key];
					break;
				}
			}

			if (!annotation) {
				comment.cellPos = this._cellCursor;
				annotation = this.createAnnotation(comment);
				annotation._annotation._tag = annotation;
				this.showAnnotation(annotation);
			}
			annotation.editAnnotation();
		}
	},

	createAnnotation: function (comment) {
		var annotation = L.divOverlay(comment.cellPos).bindAnnotation(L.annotation(L.latLng(0, 0),
			comment, comment.id === 'new' ? {noMenu: true} : {}));
		return annotation;
	},

	beforeAdd: function (map) {
		map._addZoomLimit(this);
		map.on('zoomend', this._onZoomRowColumns, this);
		map.on('updateparts', this._onUpdateParts, this);
		map.on('AnnotationCancel', this._onAnnotationCancel, this);
		map.on('AnnotationReply', this._onAnnotationReply, this);
		map.on('AnnotationSave', this._onAnnotationSave, this);
		if (L.Browser.mobile) {
			this.onMobileInit(map);
		}
	},

	clearAnnotations: function () {
		for (var tab in this._annotations) {
			this.hideAnnotations(tab);
		}
		this._annotations = {};
	},

	onAdd: function (map) {
		map.addControl(L.control.tabs());
		map.addControl(L.control.columnHeader());
		map.addControl(L.control.rowHeader());
		L.TileLayer.prototype.onAdd.call(this, map);
		this._annotations = {};
	},

	onMobileInit: function (map) {
		var toolItems = [
			{type: 'button',  id: 'showsearchbar',  img: 'search', hint: _('Show the search bar')},
			{type: 'break'},
			{type: 'button',  id: 'bold',  img: 'bold', hint: _UNO('.uno:Bold'), uno: 'Bold'},
			{type: 'button',  id: 'italic', img: 'italic', hint: _UNO('.uno:Italic'), uno: 'Italic'},
			{type: 'button',  id: 'underline',  img: 'underline', hint: _UNO('.uno:Underline'), uno: 'Underline'},
			{type: 'button',  id: 'strikeout', img: 'strikeout', hint: _UNO('.uno:Strikeout'), uno: 'Strikeout'},
			{type: 'break'},
			{type: 'button',  id: 'fontcolor', img: 'textcolor', hint: _UNO('.uno:FontColor')},
			{type: 'button',  id: 'backcolor', img: 'backcolor', hint: _UNO('.uno:BackgroundColor')},
			{type: 'button',  id: 'togglemergecells',  img: 'togglemergecells', hint: _UNO('.uno:ToggleMergeCells', 'spreadsheet', true), uno: 'ToggleMergeCells', disabled: true},
//			{type: 'break', id: 'breakmergecells'},
			{type: 'break'},
			{type: 'button', id: 'alignleft', img: 'alignleft', hint: _UNO('.uno:AlignLeft', 'spreadsheet', true), uno: 'AlignLeft'},
			{type: 'button', id: 'alignhorizontalcenter', img: 'alignhorizontal', hint: _UNO('.uno:AlignHorizontalCenter', 'spreadsheet', true), uno: 'AlignHorizontalCenter'},
			{type: 'button', id: 'alignright', img: 'alignright', hint: _UNO('.uno:AlignRight', 'spreadsheet', true), uno: 'AlignRight'},
			{type: 'button', id: 'alignblock', img: 'alignblock', hint: _UNO('.uno:AlignBlock', 'spreadsheet', true), uno: 'AlignBlock'},
			{type: 'break'},
			{type: 'button',  id: 'wraptext',  img: 'wraptext', hint: _UNO('.uno:WrapText', 'spreadsheet', true), uno: 'WrapText', disabled: true},
			{type: 'button',  id: 'insertrowsafter',  img: 'insertrowsafter', hint: _UNO('.uno:InsertRowsAfter'), uno: 'InsertRowsAfter'},
			{type: 'button',  id: 'insertcolumnsafter',  img: 'insertcolumnsafter', hint: _UNO('.uno:InsertColumnsAfter'), uno: 'InsertColumnsAfter'},
/*			{type: 'button',  id: 'numberformatcurrency',  img: 'numberformatcurrency', hint: _UNO('.uno:NumberFormatCurrency', 'spreadsheet', true), uno: 'NumberFormatCurrency', disabled: true},
			{type: 'button',  id: 'numberformatpercent',  img: 'numberformatpercent', hint: _UNO('.uno:NumberFormatPercent', 'spreadsheet', true), uno: 'NumberFormatPercent', disabled: true},
			{type: 'button',  id: 'numberformatdecdecimals',  img: 'numberformatdecdecimals', hint: _UNO('.uno:NumberFormatDecDecimals', 'spreadsheet', true), hidden: true, uno: 'NumberFormatDecDecimals', disabled: true},
			{type: 'button',  id: 'numberformatincdecimals',  img: 'numberformatincdecimals', hint: _UNO('.uno:NumberFormatIncDecimals', 'spreadsheet', true), hidden: true, uno: 'NumberFormatIncDecimals', disabled: true},
			{type: 'button',  id: 'sum',  img: 'autosum', hint: _('Sum')},
			{type: 'break',   id: 'break-number'}, */
		];

		var toolbar = $('#toolbar-up');
		toolbar.w2toolbar({
			name: 'actionbar',
			tooltip: 'bottom',
			items: [
				{type: 'button',  id: 'closemobile',  img: 'closemobile'},
				{type: 'spacer'},
				{type: 'button',  id: 'undo',  img: 'undo', hint: _UNO('.uno:Undo'), uno: 'Undo', disabled: true},
				{type: 'button',  id: 'redo',  img: 'redo', hint: _UNO('.uno:Redo'), uno: 'Redo', disabled: true},
				{type: 'button',  id: 'mobile_wizard', img: 'mobile_wizard', disabled: true},
				{type: 'button',  id: 'insertion_mobile_wizard', img: 'insertion_mobile_wizard', disabled: true},
//				{type: 'button',  id: 'insertcomment', img: 'insertcomment', disabled: true},
				{type: 'button',  id: 'fullscreen', img: 'fullscreen', hint: _UNO('.uno:FullScreen', 'text')},
				{type: 'drop', id: 'userlist', img: 'users', html: '<div id="userlist_container"><table id="userlist_table"><tbody></tbody></table>' +
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
				window.onClick(e, e.target);
				window.hideTooltip(this, e.target);
			},
			onRefresh: function() {
				L.TileLayer.prototype._onUserListRefresh(map, this);
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
				{type: 'html', id: 'formula', html: '<div id="calc-inputbar-wrapper"><div id="calc-inputbar"></div></div>'}
			],
			onClick: function (e) {
				window.onClick(e, e.target);
				window.hideTooltip(this, e.target);
			},
			onRefresh: function() {
				$('#addressInput').off('keyup', window.onAddressInput).on('keyup', window.onAddressInput);
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
			items: [{type: 'button',  id: 'insertsheet', img: 'insertsheet', hint: _('Insert sheet')}],
			onClick: function (e) {
				window.onClick(e, e.target);
				window.hideTooltip(this, e.target);
			}
		});
		toolbar.bind('touchstart', function(e) {
			w2ui['spreadsheet-toolbar'].touchStarted = true;
			var touchEvent = e.originalEvent;
			if (touchEvent && touchEvent.touches.length > 1) {
				L.DomEvent.preventDefault(e);
			}
		});
		toolbar.show();

		toolbar = $('#toolbar-down');
		toolbar.w2toolbar({
			name: 'editbar',
			tooltip: 'top',
			items: toolItems,
			onClick: function (e) {
				window.onClick(e, e.target);
				window.hideTooltip(this, e.target);
			},
			onRefresh: function(edata) {
				if (edata.target === 'insertshapes')
					window.insertShapes();
			}
		});
		toolbar.bind('touchstart', function(e) {
			w2ui['editbar'].touchStarted = true;
			var touchEvent = e.originalEvent;
			if (touchEvent && touchEvent.touches.length > 1) {
				L.DomEvent.preventDefault(e);
			}
		});

		toolbar = $('#toolbar-search');
		toolbar.w2toolbar({
			name: 'searchbar',
			tooltip: 'top',
			items: [
				{
					type: 'html', id: 'search',
					html: '<div id="search-input-group" style="padding: 3px 10px;" class="loleaflet-font">' +
						'    <label for="search-input">Search:</label>' +
						'    <input size="10" id="search-input"' +
						'style="padding: 3px; border-radius: 2px; border: 1px solid silver"/>' +
						'</div>'
				},
				{type: 'button', id: 'searchprev', img: 'prev', hint: _UNO('.uno:UpSearch'), disabled: true},
				{type: 'button', id: 'searchnext', img: 'next', hint: _UNO('.uno:DownSearch'), disabled: true},
				{type: 'button', id: 'cancelsearch', img: 'cancel', hint: _('Clear the search field'), hidden: true},
				{type: 'html', id: 'left'},
				{type: 'button', id: 'hidesearchbar', img: 'unfold', hint: _('Hide the search bar')}
			],
			onClick: function (e) {
				window.onClick(e, e.target, e.item, e.subItem);
			},
			onRefresh: function () {
				window.setupSearchInput();
			}
		});

		toolbar.bind('touchstart', function(e) {
			w2ui['searchbar'].touchStarted = true;
			var touchEvent = e.originalEvent;
			if (touchEvent && touchEvent.touches.length > 1) {
				L.DomEvent.preventDefault(e);
			}
		});

		$(w2ui.searchbar.box).find('.w2ui-scroll-left, .w2ui-scroll-right').hide();
		w2ui.searchbar.on('resize', function(target, e) {
			e.isCancelled = true;
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

		map.on('updatepermission', window.onUpdatePermission);
	},

	onAnnotationModify: function (annotation) {
		annotation.edit();
		annotation.focus();
	},

	onAnnotationRemove: function (id) {
		var comment = {
			Id: {
				type: 'string',
				value: id
			}
		};
		var tab = this._selectedPart;
		this._map.sendUnoCommand('.uno:DeleteNote', comment);
		this._annotations[tab][id].closePopup();
		this._map.focus();
	},

	onAnnotationReply: function (annotation) {
		annotation.reply();
		annotation.focus();
	},

	isCurrentCellCommentShown: function () {
		var annotations = this._annotations[this._selectedPart];
		for (var key in annotations) {
			var annotation = annotations[key]._annotation;
			if (this._cellCursor.contains(annotation._data.cellPos)) {
				return this._map.hasLayer(annotation) && annotation.isVisible();
			}
		}
		return false;
	},

	showAnnotationFromCurrentCell: function() {
		var annotations = this._annotations[this._selectedPart];
		for (var key in annotations) {
			var annotation = annotations[key]._annotation;
			if (this._cellCursor.contains(annotation._data.cellPos)) {
				this._map.addLayer(annotation);
				annotation.show();
			}
		}
	},

	hideAnnotationFromCurrentCell: function() {
		var annotations = this._annotations[this._selectedPart];
		for (var key in annotations) {
			var annotation = annotations[key]._annotation;
			if (this._cellCursor.contains(annotation._data.cellPos)) {
				annotation.hide();
				this._map.removeLayer(annotation);
			}
		}
	},

	showAnnotation: function (annotation) {
		this._map.addLayer(annotation);
	},

	hideAnnotation: function (annotation) {
		if (annotation)
			this._map.removeLayer(annotation);
	},

	showAnnotations: function () {
		var annotations = this._annotations[this._selectedPart];
		for (var key in annotations) {
			this.showAnnotation(annotations[key]);
		}
	},

	hideAnnotations: function (part) {
		var annotations = this._annotations[part];
		for (var key in annotations) {
			this.hideAnnotation(annotations[key]);
		}
	},

	isHiddenPart: function (part) {
		if (!this._hiddenParts)
			return false;
		return this._hiddenParts.indexOf(part) !== -1;
	},

	hiddenParts: function () {
		if (!this._hiddenParts)
			return 0;
		return this._hiddenParts.length;
	},

	hasAnyHiddenPart: function () {
		if (!this._hiddenParts)
			return false;
		return this.hiddenParts() !== 0;
	},
	_onAnnotationCancel: function (e) {
		if (e.annotation._data.id === 'new') {
			this.hideAnnotation(e.annotation._tag);
		} else {
			this._annotations[e.annotation._data.tab][e.annotation._data.id].closePopup();
		}
		this._map.focus();
	},

	_onAnnotationReply: function (e) {
		var comment = {
			Id: {
				type: 'string',
				value: e.annotation._data.id
			},
			Text: {
				type: 'string',
				value: e.annotation._data.reply
			}
		};
		this._map.sendUnoCommand('.uno:ReplyComment', comment);
		this._map.focus();
	},

	_onAnnotationSave: function (e) {
		var comment;
		if (e.annotation._data.id === 'new') {
			comment = {
				Text: {
					type: 'string',
					value: e.annotation._data.text
				},
				Author: {
					type: 'string',
					value: e.annotation._data.author
				}
			};
			this._map.sendUnoCommand('.uno:InsertAnnotation', comment);
			this.hideAnnotation(e.annotation._tag);
		} else {
			comment = {
				Id: {
					type: 'string',
					value: e.annotation._data.id
				},
				Text: {
					type: 'string',
					value: e.annotation._data.text
				},
				Author: {
					type: 'string',
					value: this._map.getViewName(this._viewId)
				}
			};
			this._map.sendUnoCommand('.uno:EditAnnotation', comment);
			this._annotations[e.annotation._data.tab][e.annotation._data.id].closePopup();
		}
		this._map.focus();
	},

	_onUpdateParts: function (e) {
		if (typeof this._prevSelectedPart === 'number' && !e.source) {
			this.hideAnnotations(this._prevSelectedPart);
			this.showAnnotations();
		}
	},

	_onMessage: function (textMsg, img) {
		if (textMsg.startsWith('comment:')) {
			var obj = JSON.parse(textMsg.substring('comment:'.length + 1));
			obj.comment.tab = parseInt(obj.comment.tab);
			if (obj.comment.action === 'Add') {
				obj.comment.cellPos = L.LOUtil.stringToBounds(obj.comment.cellPos);
				obj.comment.cellPos = L.latLngBounds(this._twipsToLatLng(obj.comment.cellPos.getBottomLeft()),
					this._twipsToLatLng(obj.comment.cellPos.getTopRight()));
				if (!this._annotations[obj.comment.tab]) {
					this._annotations[obj.comment.tab] = {};
				}
				this._annotations[obj.comment.tab][obj.comment.id] = this.createAnnotation(obj.comment);
				if (obj.comment.tab === this._selectedPart) {
					this.showAnnotation(this._annotations[obj.comment.tab][obj.comment.id]);
				}
			} else if (obj.comment.action === 'Remove') {
				var removed = this._annotations[obj.comment.tab][obj.comment.id];
				if (removed) {
					this.hideAnnotation(removed);
					delete this._annotations[obj.comment.tab][obj.comment.id];
				}
			} else if (obj.comment.action === 'Modify') {
				var modified = this._annotations[obj.comment.tab][obj.comment.id];
				obj.comment.cellPos = L.LOUtil.stringToBounds(obj.comment.cellPos);
				obj.comment.cellPos = L.latLngBounds(this._twipsToLatLng(obj.comment.cellPos.getBottomLeft()),
					this._twipsToLatLng(obj.comment.cellPos.getTopRight()));
				if (modified) {
					modified._annotation._data = obj.comment;
					modified.setLatLngBounds(obj.comment.cellPos);
				}
			}
		} else if (textMsg.startsWith('invalidateheader: column')) {
			this._map.fire('updaterowcolumnheaders', {x: this._map._getTopLeftPoint().x, y: 0, offset: {x: undefined, y: 0}});
			this._map._socket.sendMessage('commandvalues command=.uno:ViewAnnotationsPosition');
		} else if (textMsg.startsWith('invalidateheader: row')) {
			this._map.fire('updaterowcolumnheaders', {x: 0, y: this._map._getTopLeftPoint().y, offset: {x: 0, y: undefined}});
			this._map._socket.sendMessage('commandvalues command=.uno:ViewAnnotationsPosition');
		} else if (textMsg.startsWith('invalidateheader: all')) {
			this._map.fire('updaterowcolumnheaders', {x: this._map._getTopLeftPoint().x, y: this._map._getTopLeftPoint().y, offset: {x: undefined, y: undefined}});
			this._map._socket.sendMessage('commandvalues command=.uno:ViewAnnotationsPosition');
		} else {
			L.TileLayer.prototype._onMessage.call(this, textMsg, img);
		}
	},

	_onInvalidateTilesMsg: function (textMsg) {
		var command = this._map._socket.parseServerCmd(textMsg);
		if (command.x === undefined || command.y === undefined || command.part === undefined) {
			var strTwips = textMsg.match(/\d+/g);
			command.x = parseInt(strTwips[0]);
			command.y = parseInt(strTwips[1]);
			command.width = parseInt(strTwips[2]);
			command.height = parseInt(strTwips[3]);
			command.part = this._selectedPart;
		}
		var topLeftTwips = new L.Point(command.x, command.y);
		var offset = new L.Point(command.width, command.height);
		var bottomRightTwips = topLeftTwips.add(offset);
		if (this._debug) {
			this._debugAddInvalidationRectangle(topLeftTwips, bottomRightTwips, textMsg);
		}
		var invalidBounds = new L.Bounds(topLeftTwips, bottomRightTwips);
		var visibleTopLeft = this._latLngToTwips(this._map.getBounds().getNorthWest());
		var visibleBottomRight = this._latLngToTwips(this._map.getBounds().getSouthEast());
		var visibleArea = new L.Bounds(visibleTopLeft, visibleBottomRight);

		var needsNewTiles = false;
		for (var key in this._tiles) {
			var coords = this._tiles[key].coords;
			var tileTopLeft = this._coordsToTwips(coords);
			var tileBottomRight = new L.Point(this._tileWidthTwips, this._tileHeightTwips);
			var bounds = new L.Bounds(tileTopLeft, tileTopLeft.add(tileBottomRight));
			if (coords.part === command.part && invalidBounds.intersects(bounds)) {
				if (this._tiles[key]._invalidCount) {
					this._tiles[key]._invalidCount += 1;
				}
				else {
					this._tiles[key]._invalidCount = 1;
				}
				if (visibleArea.intersects(bounds)) {
					needsNewTiles = true;
					if (this._debug) {
						this._debugAddInvalidationData(this._tiles[key]);
					}
				}
				else {
					// tile outside of the visible area, just remove it
					this._removeTile(key);
				}
			}
		}

		if (needsNewTiles && command.part === this._selectedPart && this._debug)
		{
			this._debugAddInvalidationMessage(textMsg);
		}

		for (key in this._tileCache) {
			// compute the rectangle that each tile covers in the document based
			// on the zoom level
			coords = this._keyToTileCoords(key);
			if (coords.part !== command.part) {
				continue;
			}
			var scale = this._map.getZoomScale(coords.z);
			topLeftTwips = new L.Point(
					this.options.tileWidthTwips / scale * coords.x,
					this.options.tileHeightTwips / scale * coords.y);
			bottomRightTwips = topLeftTwips.add(new L.Point(
					this.options.tileWidthTwips / scale,
					this.options.tileHeightTwips / scale));
			bounds = new L.Bounds(topLeftTwips, bottomRightTwips);
			if (invalidBounds.intersects(bounds)) {
				delete this._tileCache[key];
			}
		}

		this._previewInvalidations.push(invalidBounds);
		// 1s after the last invalidation, update the preview
		clearTimeout(this._previewInvalidator);
		this._previewInvalidator = setTimeout(L.bind(this._invalidatePreviews, this), this.options.previewInvalidationTimeout);
	},

	_onSetPartMsg: function (textMsg) {
		var part = parseInt(textMsg.match(/\d+/g)[0]);
		if (part !== this._selectedPart && !this.isHiddenPart(part)) {
			this._map.setPart(part, true);
			this._map.fire('setpart', {selectedPart: this._selectedPart});
			// TODO: test it!
			this._map.fire('updaterowcolumnheaders');
		}
	},

	_onZoomRowColumns: function () {
		this._sendClientZoom();
		// TODO: test it!
		this._map.fire('updaterowcolumnheaders');
		this._map._socket.sendMessage('commandvalues command=.uno:ViewAnnotationsPosition');
	},

	_onUpdateCurrentHeader: function() {
		var x = -1, y = -1;
		if (this._cellCursorXY) {
			x = this._cellCursorXY.x + 1;
			y = this._cellCursorXY.y + 1;
		}
		var size = new L.Point(0, 0);
		if (this._cellCursor && !this._isEmptyRectangle(this._cellCursor)) {
			size = this._cellCursorTwips.getSize();
		}
		this._map.fire('updatecurrentheader', {curX: x, curY: y, width: size.x, height: size.y});
	},

	_onUpdateSelectionHeader: function () {
		var layers = this._selections.getLayers();
		var layer = layers.pop();
		if (layers.length === 0 && layer && layer.getLatLngs().length === 1) {
			var start = this._latLngToTwips(layer.getBounds().getNorthWest()).add([1, 1]);
			var end = this._latLngToTwips(layer.getBounds().getSouthEast()).subtract([1, 1]);
			this._map.fire('updateselectionheader', {start: start, end: end});
		}
		else {
			this._map.fire('clearselectionheader');
		}
	},

	_onStatusMsg: function (textMsg) {
		var command = this._map._socket.parseServerCmd(textMsg);
		if (command.width && command.height && this._documentInfo !== textMsg) {
			this._docWidthTwips = command.width;
			this._docHeightTwips = command.height;
			this._docType = command.type;
			this._parts = command.parts;
			this._selectedPart = command.selectedPart;
			this._viewId = parseInt(command.viewid);
			var mapSize = this._map.getSize();
			var width = this._docWidthTwips / this._tileWidthTwips * this._tileSize;
			var height = this._docHeightTwips / this._tileHeightTwips * this._tileSize;
			if (width < mapSize.x || height < mapSize.y) {
				width = Math.max(width, mapSize.x);
				height = Math.max(height, mapSize.y);
				var topLeft = this._map.unproject(new L.Point(0, 0));
				var bottomRight = this._map.unproject(new L.Point(width, height));
				this._map.setMaxBounds(new L.LatLngBounds(topLeft, bottomRight));
				this._docPixelSize = {x: width, y: height};
				this._map.fire('docsize', {x: width, y: height});
			}
			else {
				this._updateMaxBounds(true);
			}
			this._hiddenParts = command.hiddenparts || [];
			this._documentInfo = textMsg;
			var partNames = textMsg.match(/[^\r\n]+/g);
			// only get the last matches
			this._partNames = partNames.slice(partNames.length - this._parts);
			this._map.fire('updateparts', {
				selectedPart: this._selectedPart,
				parts: this._parts,
				docType: this._docType,
				partNames: this._partNames,
				hiddenParts: this._hiddenParts,
				source: 'status'
			});
			this._resetPreFetching(true);
			this._update();
		}
	},

	_onCommandValuesMsg: function (textMsg) {
		var jsonIdx = textMsg.indexOf('{');
		if (jsonIdx === -1)
			return;

		var values = JSON.parse(textMsg.substring(jsonIdx));
		if (!values) {
			return;
		}

		var comment;
		if (values.commandName === '.uno:ViewRowColumnHeaders') {
//			console.log('view row column headers: ' + JSON.stringify(values));
			this._map.fire('viewrowcolumnheaders', {
				data: values,
				converter: this._twipsToPixels,
				context: this
			});
			this._onUpdateCurrentHeader();
			this._onUpdateSelectionHeader();
		} else if (values.comments) {
			this.clearAnnotations();
			for (var index in values.comments) {
				comment = values.comments[index];
				comment.tab = parseInt(comment.tab);
				comment.cellPos = L.LOUtil.stringToBounds(comment.cellPos);
				comment.cellPos = L.latLngBounds(this._twipsToLatLng(comment.cellPos.getBottomLeft()),
					this._twipsToLatLng(comment.cellPos.getTopRight()));
				if (!this._annotations[comment.tab]) {
					this._annotations[comment.tab] = {};
				}
				this._annotations[comment.tab][comment.id] = this.createAnnotation(comment);
			}
			this.showAnnotations();
		} else if (values.commentsPos) {
			this.hideAnnotations();
			for (index in values.commentsPos) {
				comment = values.commentsPos[index];
				comment.tab = parseInt(comment.tab);
				comment.cellPos = L.LOUtil.stringToBounds(comment.cellPos);
				comment.cellPos = L.latLngBounds(this._twipsToLatLng(comment.cellPos.getBottomLeft()),
					this._twipsToLatLng(comment.cellPos.getTopRight()));
				var annotation = this._annotations[comment.tab][comment.id];
				if (annotation) {
					annotation.setLatLngBounds(comment.cellPos);
					if (annotation.mark) {
						annotation.mark.setLatLng(comment.cellPos.getNorthEast());
					}
				}
			}
			this.showAnnotations();
		} else {
			L.TileLayer.prototype._onCommandValuesMsg.call(this, textMsg);
		}
	},

	_onTextSelectionMsg: function (textMsg) {
		L.TileLayer.prototype._onTextSelectionMsg.call(this, textMsg);
		this._onUpdateSelectionHeader();
	},

	_onCellCursorMsg: function (textMsg) {
		L.TileLayer.prototype._onCellCursorMsg.call(this, textMsg);
		this._onUpdateCurrentHeader();
	}
});
