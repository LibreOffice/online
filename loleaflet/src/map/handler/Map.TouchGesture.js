/* -*- js-indent-level: 8; fill-column: 100 -*- */
/*
 * L.Map.CalcTap is used to enable mobile taps.
 */

L.Map.mergeOptions({
	touchGesture: true,
});

/* global Hammer $ */
L.Map.TouchGesture = L.Handler.extend({
	statics: {
		MAP: 1,
		CURSOR: 2,
		GRAPHIC: 4,
		MARKER: 8,
		TABLE: 16
	},

	initialize: function (map) {
		L.Handler.prototype.initialize.call(this, map);
		this._state = L.Map.TouchGesture.MAP;

		if (window.ThisIsTheiOSApp && !this._toolbar) {
			this._toolbar = L.control.contextToolbar();
			this._toolbarAdded = 0;
		}

		if (!this._hammer) {
			this._hammer = new Hammer(this._map._mapPane);
			this._hammer.get('swipe').set({
				direction: Hammer.DIRECTION_ALL
			});
			this._hammer.get('pan').set({
				direction: Hammer.DIRECTION_ALL
			});
			this._hammer.get('pinch').set({
				enable: true
			});
			// avoid to trigger the context menu too early so the user can start panning in a relaxed way
			this._hammer.get('press').set({
				time: 500
			});

			var singleTap = this._hammer.get('tap');
			var doubleTap = this._hammer.get('doubletap');
			var tripleTap = new Hammer.Tap({event: 'tripletap', taps: 3 });
			this._hammer.add(tripleTap);
			tripleTap.recognizeWith([doubleTap, singleTap]);

			if (L.Browser.touch) {
				L.DomEvent.on(this._map._mapPane, 'touchstart touchmove touchend touchcancel', L.DomEvent.preventDefault);
			}

			if (Hammer.prefixed(window, 'PointerEvent') !== undefined) {
				L.DomEvent.on(this._map._mapPane, 'pointerdown pointermove pointerup pointercancel', L.DomEvent.preventDefault);
			}

			// IE10 has prefixed support, and case-sensitive
			if (window.MSPointerEvent && !window.PointerEvent) {
				L.DomEvent.on(this._map._mapPane, 'MSPointerDown MSPointerMove MSPointerUp MSPointerCancel', L.DomEvent.preventDefault);
			}

			L.DomEvent.on(this._map._mapPane, 'mousedown mousemove mouseup', L.DomEvent.preventDefault);
			L.DomEvent.on(document, 'touchmove', L.DomEvent.preventDefault);
		}

		for (var events in L.Draggable.MOVE) {
			L.DomEvent.on(document, L.Draggable.END[events], this._onDocUp, this);
		}

		/// $.contextMenu does not support touch events so,
		/// attach 'touchend' menu clicks event handler
		if (this._hammer.input instanceof Hammer.TouchInput) {
			var $doc = $(document);
			$doc.on('click.contextMenu', '.context-menu-item', function (e) {
				var $elem = $(this);

				if ($elem.data().contextMenu.selector === '.leaflet-layer') {
					$.contextMenu.handle.itemClick.apply(this, [e]);
				}
			});
		}
	},

	addHooks: function () {
		this._hammer.on('hammer.input', L.bind(this._onHammer, this));
		this._hammer.on('tap', L.bind(this._onTap, this));
		this._hammer.on('panstart', L.bind(this._onPanStart, this));
		this._hammer.on('pan', L.bind(this._onPan, this));
		this._hammer.on('panend', L.bind(this._onPanEnd, this));
		this._hammer.on('pinchstart', L.bind(this._onPinchStart, this));
		this._hammer.on('pinchmove', L.bind(this._onPinch, this));
		this._hammer.on('pinchend', L.bind(this._onPinchEnd, this));
		this._hammer.on('tripletap', L.bind(this._onTripleTap, this));
		this._map.on('updatepermission', this._onPermission, this);
		this._onPermission({perm: this._map._permission});
	},

	removeHooks: function () {
		this._hammer.off('hammer.input', L.bind(this._onHammer, this));
		this._hammer.off('tap', L.bind(this._onTap, this));
		this._hammer.off('panstart', L.bind(this._onPanStart, this));
		this._hammer.off('pan', L.bind(this._onPan, this));
		this._hammer.off('panend', L.bind(this._onPanEnd, this));
		this._hammer.off('pinchstart', L.bind(this._onPinchStart, this));
		this._hammer.off('pinchmove', L.bind(this._onPinch, this));
		this._hammer.off('pinchend', L.bind(this._onPinchEnd, this));
		this._hammer.off('doubletap', L.bind(this._onDoubleTap, this));
		this._hammer.off('press', L.bind(this._onPress, this));
		this._hammer.off('tripletap', L.bind(this._onTripleTap, this));
		this._map.off('updatepermission', this._onPermission, this);
	},

	_onPermission: function (e) {
		if (e.perm == 'edit') {
			this._hammer.on('doubletap', L.bind(this._onDoubleTap, this));
			this._hammer.on('press', L.bind(this._onPress, this));
		} else {
			this._hammer.off('doubletap', L.bind(this._onDoubleTap, this));
			this._hammer.off('press', L.bind(this._onPress, this));
		}
	},

	_onHammer: function (e) {
		this._map.notifyActive();

		// Function/Formula Wizard keeps the formula cell active all the time,
		// so the usual range selection doesn't work here.
		// Instead, the cells are highlighted with a certain color and opacity
		// to mark as selection. And that's why we are checking for it here.
		// FIXME: JS-ify. This code is written by a C++ dev.
		function getFuncWizRangeBounds (obj) {
			for (var i in obj._map._layers) {
				if (obj._map._layers[i].options && obj._map._layers[i].options.fillColor
					&& obj._map._layers[i].options.fillOpacity) {
					if (obj._map._layers[i].options.fillColor === '#ef0fff'
						&& obj._map._layers[i].options.fillOpacity === 0.25) {
						return obj._map._layers[i]._bounds;
					}
				}
			}
		}

		if (e.isFirst) {
			var point = e.pointers[0],
			    containerPoint = this._map.mouseEventToContainerPoint(point),
			    layerPoint = this._map.containerPointToLayerPoint(containerPoint),
			    latlng = this._map.layerPointToLatLng(layerPoint),
			funcWizardRangeBounds = getFuncWizRangeBounds(this);

			if (this._map._docLayer._graphicMarker) {
				this._marker = this._map._docLayer._graphicMarker.transform.getMarker(layerPoint);
			}

			if (this._marker) {
				this._state = L.Map.TouchGesture.MARKER;
			} else if (this._map._docLayer._graphicMarker && this._map._docLayer._graphicMarker.getBounds().contains(latlng)) {
				if (this._map._docLayer.hasTableSelection())
					this._state = L.Map.TouchGesture.TABLE;
				else
					this._state = L.Map.TouchGesture.GRAPHIC;
			} else if (this._map._docLayer._cellCursor && this._map._docLayer._cellCursor.contains(latlng)) {
				this._state = L.Map.TouchGesture.CURSOR;
			} else if (this._map._docLayer._cellCursor && funcWizardRangeBounds && funcWizardRangeBounds.contains(latlng)) {
				this._state = L.Map.TouchGesture.CURSOR;
			} else {
				this._state = L.Map.TouchGesture.MAP;
			}
			this._moving = false;
		}

		if (e.isLast && this._state !== L.Map.TouchGesture.MAP) {
			this._state = L.Map.TouchGesture.hitTest.MAP;
			this._marker = undefined;
			this._moving = false;
		}

		if ($(e.srcEvent.target).has(this._map._mapPane)) {
			L.DomEvent.preventDefault(e.srcEvent);
			L.DomEvent.stopPropagation(e.srcEvent);
		}
	},

	_onDocUp: function () {
		if (!this._map.touchGesture.enabled()) {
			this._map.touchGesture.enable();
		}
	},

	_onPress: function (e) {
		var point = e.pointers[0],
		    containerPoint = this._map.mouseEventToContainerPoint(point),
		    layerPoint = this._map.containerPointToLayerPoint(containerPoint),
		    latlng = this._map.layerPointToLatLng(layerPoint),
		    mousePos = this._map._docLayer._latLngToTwips(latlng);

		if (this._moving) {
			return;
		}

		this._map.fire('closepopups');

		var that = this;
		var docLayer = this._map._docLayer;

		if (window.ThisIsTheiOSApp) {
			// console.log('==> ' + e.timeStamp);
			if (!this._toolbar._map && (docLayer.containsSelection(latlng) || (docLayer._graphicSelection && docLayer._graphicSelection.contains(latlng)))) {
				this._toolbar._pos = containerPoint;
				// console.log('==> Adding context toolbar ' + e.timeStamp);
				this._toolbar.addTo(this._map);
				this._toolbarAdded = e.timeStamp;
			} else if (this._toolbarAdded && e.timeStamp - this._toolbarAdded >= 1000) {
				// console.log('==> Removing context toolbar ' + e.timeStamp);
				this._toolbar.remove();
				this._map._contextMenu._onMouseDown({originalEvent: e.srcEvent});
				// send right click to trigger context menus
				this._map._docLayer._postMouseEvent('buttondown', mousePos.x, mousePos.y, 1, 4, 0);
				this._map._docLayer._postMouseEvent('buttonup', mousePos.x, mousePos.y, 1, 4, 0);
			}
		} else {
			var singleClick = function () {
				docLayer._postMouseEvent('buttondown', mousePos.x, mousePos.y, 1, 1, 0);
				docLayer._postMouseEvent('buttonup', mousePos.x, mousePos.y, 1, 1, 0);
			};

			var doubleClick = function () {
				docLayer._postMouseEvent('buttondown', mousePos.x, mousePos.y, 2, 1, 0);
				docLayer._postMouseEvent('buttonup', mousePos.x, mousePos.y, 2, 1, 0);
			};

			var rightClick = function () {
				docLayer._postMouseEvent('buttondown', mousePos.x, mousePos.y, 1, 4, 0);
				docLayer._postMouseEvent('buttonup', mousePos.x, mousePos.y, 1, 4, 0);
			};

			var waitForSelectionMsg = function () {
				// check new selection if any
				var graphicSelection = docLayer._graphicSelection;
				var cellCursor = docLayer._cellCursor;
				if (!docLayer._cursorAtMispelledWord
					&& (!graphicSelection || !graphicSelection.contains(latlng))
					&& (!cellCursor || !cellCursor.contains(latlng))) {
					// try to select text
					doubleClick();
				}
				// send right click to trigger context menus
				that._map._contextMenu._onMouseDown({originalEvent: e.srcEvent});
				rightClick();
			};

			// we want to select the long touched object before triggering the context menu;
			// for selecting text we need to simulate a double click, anyway for a graphic object
			// a single click is enough, while a double click can lead to switch to edit mode
			// (not only for an embedded ole object, even for entering text inside a shape);
			// a similar problem regards spreadsheet cell: a single click moves the cell cursor,
			// while a double click enables text input;
			// in order to avoid these cases, we send a single click and wait for a few milliseconds
			// before checking if we received a possible selection message; if no such message is received
			// we simulate a double click for trying to select text and finally, in any case,
			// we trigger the context menu by sending a right click
			var graphicSelection = docLayer._graphicSelection;
			var cellCursor = docLayer._cellCursor;
			var textSelection;
			if (docLayer._textSelectionStart && docLayer._textSelectionEnd)
				textSelection = new L.LatLngBounds(docLayer._textSelectionStart.getSouthWest(), docLayer._textSelectionEnd.getNorthEast());

			if ((textSelection && textSelection.inBand(latlng))
				|| (graphicSelection && graphicSelection.contains(latlng))
				|| (cellCursor && cellCursor.contains(latlng))) {
				// long touched an already selected object
				// send right click to trigger context menus
				this._map._contextMenu._onMouseDown({originalEvent: e.srcEvent});
				rightClick();
			}
			else {
				// try to select a graphic object or move the cell cursor
				singleClick();
				setTimeout(waitForSelectionMsg, 300);
			}
		}

		this._map.notifyActive();
		e.preventDefault();
	},

	_onTap: function (e) {
		var point = e.pointers[0],
		    containerPoint = this._map.mouseEventToContainerPoint(point),
		    layerPoint = this._map.containerPointToLayerPoint(containerPoint),
		    latlng = this._map.layerPointToLatLng(layerPoint),
		    mousePos = this._map._docLayer._latLngToTwips(latlng);

		if (window.ThisIsTheiOSApp)
			this._toolbar.remove();

		// clicked a hyperlink popup - not really designed for this.
		if (this._map.hyperlinkPopup && e.target &&
		    this._map.hyperlinkPopup._contentNode == e.target.parentNode)
			this._map.fire('hyperlinkclicked', {url: e.target.href});

		this._map.fire('closepopups');
		this._map.fire('closemobilewizard');
		this._map.fire('editorgotfocus');

		var docLayer = this._map._docLayer;
		// unselect if anything is selected already
		if (docLayer && docLayer._annotations && docLayer._annotations.unselect) {
			docLayer._annotations.unselect();
			var pointPx = docLayer._twipsToPixels(mousePos);
			var bounds = docLayer._annotations.getBounds();
			if (bounds && bounds.contains(pointPx)) {
				// not forward mouse events to core if the user tap on a comment box
				// for instance on Writer that causes the text cursor to be moved
				return;
			}
		}
		this._map._contextMenu._onMouseDown({originalEvent: e.srcEvent});

		if (docLayer) {
			docLayer._postMouseEvent('buttondown', mousePos.x, mousePos.y, 1, 1, 0);
			docLayer._postMouseEvent('buttonup', mousePos.x, mousePos.y, 1, 1, 0);

			// Take focus, but keyboard show only in Writer (double-tap to edit Calc/Impress).
			this._map.focus(this._map._docLayer._docType === 'text');
		}
	},

	_onDoubleTap: function (e) {
		var point = e.pointers[0],
		    containerPoint = this._map.mouseEventToContainerPoint(point),
		    layerPoint = this._map.containerPointToLayerPoint(containerPoint),
		    latlng = this._map.layerPointToLatLng(layerPoint),
		    mousePos = this._map._docLayer._latLngToTwips(latlng);

		var docLayer = this._map._docLayer;
		if (docLayer) {
			docLayer._postMouseEvent('buttondown', mousePos.x, mousePos.y, 2, 1, 0);
			docLayer._postMouseEvent('buttonup', mousePos.x, mousePos.y, 2, 1, 0);

			// Show keyboard.
			this._map.focus(true);
		}
	},

	_onTripleTap: function (e) {
		var point = e.pointers[0],
		    containerPoint = this._map.mouseEventToContainerPoint(point),
		    layerPoint = this._map.containerPointToLayerPoint(containerPoint),
		    latlng = this._map.layerPointToLatLng(layerPoint),
		    mousePos = this._map._docLayer._latLngToTwips(latlng);

		this._map._docLayer._postMouseEvent('buttondown', mousePos.x, mousePos.y, 1, 1, 8192);
		this._map._docLayer._postMouseEvent('buttonup', mousePos.x, mousePos.y, 1, 1, 8192);
	},

	_onPanStart: function (e) {
		var point = e.pointers[0],
		    containerPoint = this._map.mouseEventToContainerPoint(point),
		    layerPoint = this._map.containerPointToLayerPoint(containerPoint),
		    latlng = this._map.layerPointToLatLng(layerPoint),
		    mousePos = this._map._docLayer._latLngToTwips(latlng);

		var originalCellCursor = this._map._docLayer._cellCursor;
		var increaseRatio = 0.40;
		var increasedCellCursor = null;
		if (originalCellCursor) {
			increasedCellCursor = originalCellCursor.padVertically(increaseRatio);
		}

		if (increasedCellCursor && increasedCellCursor.contains(latlng)) {
			if (!originalCellCursor.contains(latlng)) {
				var lat = latlng.lat;
				var lng = latlng.lng;

				var sw = originalCellCursor._southWest,
				ne = originalCellCursor._northEast;
				var heightBuffer = Math.abs(sw.lat - ne.lat) * increaseRatio;

				if (lat < originalCellCursor.getSouthWest().lat) {
					lat = lat + heightBuffer;
				}

				if (lat > originalCellCursor.getNorthEast().lat) {
					lat = lat - heightBuffer;
				}

				latlng = new L.LatLng(lat, lng);
				mousePos = this._map._docLayer._latLngToTwips(latlng);
			}
		}

		if (this._state === L.Map.TouchGesture.MARKER) {
			this._map._fireDOMEvent(this._marker, point, 'mousedown');
		} else if (this._state === L.Map.TouchGesture.TABLE) {
			this._map._docLayer._postMouseEvent('buttondown', mousePos.x, mousePos.y, 1, 1, 0);
		} else if (this._state === L.Map.TouchGesture.GRAPHIC) {
			var mouseEvent = this._map._docLayer._createNewMouseEvent('mousedown', point);
			this._map._docLayer._graphicMarker._onDragStart(mouseEvent);
		} else if (this._state === L.Map.TouchGesture.CURSOR) {
			this._map._docLayer._postMouseEvent('buttondown', mousePos.x, mousePos.y, 1, 1, 0);
		} else {
			this._map.dragging._draggable._onDown(this._constructFakeEvent(point, 'mousedown'));
		}
	},

	_onPan: function (e) {
		var point = e.pointers[0],
		    containerPoint = this._map.mouseEventToContainerPoint(point),
		    layerPoint = this._map.containerPointToLayerPoint(containerPoint),
		    latlng = this._map.layerPointToLatLng(layerPoint),
		    mousePos = this._map._docLayer._latLngToTwips(latlng);

		if (this._state === L.Map.TouchGesture.MARKER) {
			this._map._fireDOMEvent(this._map, point, 'mousemove');
			this._moving = true;
		} else if (this._state === L.Map.TouchGesture.GRAPHIC) {
			var mouseEvent = this._map._docLayer._createNewMouseEvent('mousemove', point);
			this._map._docLayer._graphicMarker._onDrag(mouseEvent);
			this._moving = true;
		} else if (this._state === L.Map.TouchGesture.TABLE) {
			this._map._docLayer._postMouseEvent('move', mousePos.x, mousePos.y, 1, 1, 0);
			this._moving = true;
		} else if (this._state === L.Map.TouchGesture.CURSOR) {
			this._map._docLayer._postMouseEvent('move', mousePos.x, mousePos.y, 1, 1, 0);
		} else {
			this._map.dragging._draggable._onMove(this._constructFakeEvent(point, 'mousemove'));
		}
	},

	_onPanEnd: function (e) {
		var point = e.pointers[0],
		    containerPoint = this._map.mouseEventToContainerPoint(point),
		    layerPoint = this._map.containerPointToLayerPoint(containerPoint),
		    latlng = this._map.layerPointToLatLng(layerPoint),
		    mousePos = this._map._docLayer._latLngToTwips(latlng);

		if (this._state === L.Map.TouchGesture.MARKER) {
			this._map._fireDOMEvent(this._map, point, 'mouseup');
			this._moving = false;
		} else if (this._state === L.Map.TouchGesture.GRAPHIC) {
			var mouseEvent = this._map._docLayer._createNewMouseEvent('mouseup', point);
			this._map._docLayer._graphicMarker._onDragEnd(mouseEvent);
			this._moving = false;
		} else if (this._state === L.Map.TouchGesture.TABLE) {
			this._map._docLayer._postMouseEvent('buttonup', mousePos.x, mousePos.y, 1, 1, 0);
			this._moving = false;
		} else if (this._state === L.Map.TouchGesture.CURSOR) {
			this._map._docLayer._postMouseEvent('buttonup', mousePos.x, mousePos.y, 1, 1, 0);
		} else {
			this._map.dragging._draggable._onUp(this._constructFakeEvent(point, 'mouseup'));
		}
	},

	_onPinchStart: function (e) {
		this._pinchStartCenter = {x: e.center.x, y: e.center.y};
		if (this._map._docLayer.isCursorVisible()) {
			this._map._docLayer._cursorMarker.setOpacity(0);
		}
		if (this._map._textInput._cursorHandler) {
			this._map._textInput._cursorHandler.setOpacity(0);
		}
		if (this._map._docLayer._selectionHandles['start']) {
			this._map._docLayer._selectionHandles['start'].setOpacity(0);
		}
		if (this._map._docLayer._selectionHandles['end']) {
			this._map._docLayer._selectionHandles['end'].setOpacity(0);
		}
		this._map._docLayer.eachView(this._map._docLayer._viewCursors, function (item) {
			var viewCursorMarker = item.marker;
			if (viewCursorMarker) {
				viewCursorMarker.setOpacity(0);
			}
		}, this._map._docLayer, true);
	},

	_onPinch: function (e) {
		if (!this._pinchStartCenter)
			return;

		// we need to invert the offset or the map is moved in the opposite direction
		var offset = {x: e.center.x - this._pinchStartCenter.x, y: e.center.y - this._pinchStartCenter.y};
		var center = {x: this._pinchStartCenter.x - offset.x, y: this._pinchStartCenter.y - offset.y};
		this._zoom = this._map.getScaleZoom(e.scale);
		this._center = this._map._limitCenter(this._map.mouseEventToLatLng({clientX: center.x, clientY: center.y}),
						      this._zoom, this._map.options.maxBounds);

		L.Util.cancelAnimFrame(this._animRequest);
		this._animRequest = L.Util.requestAnimFrame(function () {
			this._map._animateZoom(this._center, this._zoom, false, true);
		}, this, true, this._map._container);
	},

	_onPinchEnd: function () {
		var oldZoom = this._map.getZoom(),
		    zoomDelta = this._zoom - oldZoom,
		    finalZoom = this._map._limitZoom(zoomDelta > 0 ? Math.ceil(this._zoom) : Math.floor(this._zoom));

		if (this._map._docLayer.isCursorVisible()) {
			this._map._docLayer._cursorMarker.setOpacity(1);
		}
		if (this._map._textInput._cursorHandler) {
			this._map._textInput._cursorHandler.setOpacity(1);
		}
		if (this._map._docLayer._selectionHandles['start']) {
			this._map._docLayer._selectionHandles['start'].setOpacity(1);
		}
		if (this._map._docLayer._selectionHandles['end']) {
			this._map._docLayer._selectionHandles['end'].setOpacity(1);
		}

		if (this._center) {
			L.Util.cancelAnimFrame(this._animRequest);
			this._map._animateZoom(this._center, finalZoom, true, true);
		}

		if (this._map._docLayer && this._map._docLayer._annotations) {
			var annotations = this._map._docLayer._annotations;
			if (annotations.update)
				setTimeout(function() {
					annotations.update();
				}, 250 /* ms */);
		}
	},

	_constructFakeEvent: function (evt, type) {
		var fakeEvt = {
			type: type,
			canBubble: false,
			cancelable: true,
			screenX: evt.screenX,
			screenY: evt.screenY,
			clientX: evt.clientX,
			clientY: evt.clientY,
			ctrlKey: false,
			altKey: false,
			shiftKey: false,
			metaKey: false,
			button: 0,
			target: evt.target,
			preventDefault: function () {}
		};

		return fakeEvt;
	}
});
