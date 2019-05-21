/* -*- js-indent-level: 8 -*- */
/*
 * L.Map.Mouse is handling mouse interaction with the document
 */

L.Map.mergeOptions({
	mouse: true
});

L.Map.Mouse = L.Handler.extend({

	initialize: function (map) {
		this._map = map;
		this._mouseEventsQueue = [];
		this._prevMousePos = null;
		this._prevMouseButtons = null;
	},

	addHooks: function () {
		this._map.on('mousedown mouseup mouseover mouseout mousemove dblclick trplclick qdrplclick',
			this._onMouseEvent, this);

		L.DomEvent.on(this._map.getContainer(), 'contextmenu', this._onContextMenu, this);
	},

	removeHooks: function () {
		this._map.off('mousedown mouseup mouseover mouseout mousemove dblclick trplclick qdrplclick',
			this._onMouseEvent, this);

		L.DomEvent.off(this._map.getContainer(), 'contextmenu', this._onContextMenu, this);
	},

	LOButtons: {
		left: 1,
		middle: 2,
		right: 4
	},

	// See values from https://developer.mozilla.org/en-US/docs/Web/API/MouseEvent/button
	JSButtons: {
		left: 0,
		middle: 1,
		right: 2
	},

	_onContextMenu: function(ev) {
		this._stopQueue();

		var mapCoords = this._map.mouseEventToLatLng(ev);
		if (!this._prevMousePos) {
			this._prevMousePos = mapCoords;
		}
		var twipCoords = this._map._docLayer._latLngToTwips(mapCoords);

		this._queueMouseEvent(['contextmenu', twipCoords.x, twipCoords.y], 100);
	},

	_onMouseEvent: function (e) {
		var docLayer = this._map._docLayer;
		if (!docLayer || (this._map.slideShow && this._map.slideShow.fullscreen) || this._map.rulerActive) {
			return;
		}
		if (docLayer._graphicMarker) {
			if (docLayer._graphicMarker.isDragged) {
				return;
			}
			if (!docLayer._isEmptyRectangle(docLayer._graphicSelection)) {
				// if we have a graphic selection and the user clicks inside the rectangle
				var isInside = docLayer._graphicMarker.getBounds().contains(e.latlng);
				if (e.type === 'mousedown' && isInside) {
					this._prevMousePos = e.latlng;
				}
				else if (e.type === 'mousemove' && this._mouseDown) {
					if (!this._prevMousePos && isInside) {
						// if the user started to drag the shape before the selection
						// has been drawn
						this._prevMousePos = e.latlng;
					}
					else {
						this._prevMousePos = e.latlng;
					}
				}
				else if (e.type === 'mouseup') {
					this._prevMousePos = null;
				}
			}
		}

		for (var key in docLayer._selectionHandles) {
			if (docLayer._selectionHandles[key].isDragged) {
				return;
			}
		}

		var modifier = 0;
		var shift = e.originalEvent.shiftKey ? this._map.keyboard.keyModifier.shift : 0;
		var ctrl = e.originalEvent.ctrlKey ? this._map.keyboard.keyModifier.ctrl : 0;
		var alt = e.originalEvent.altKey ? this._map.keyboard.keyModifier.alt : 0;
		var cmd = e.originalEvent.metaKey ? this._map.keyboard.keyModifier.ctrlMac : 0;
		modifier = shift | ctrl | alt | cmd;

		var buttons = 0;
		buttons |= e.originalEvent.button === this.JSButtons.left ? this.LOButtons.left : 0;
		buttons |= e.originalEvent.button === this.JSButtons.middle ? this.LOButtons.middle : 0;
		buttons |= e.originalEvent.button === this.JSButtons.right ? this.LOButtons.right : 0;

		var mouseEnteringLeavingMap = this._map._mouseEnteringLeaving;

		if (mouseEnteringLeavingMap && e.type === 'mouseover' && this._mouseDown) {
			L.DomEvent.off(document, 'mousemove', this._onMouseMoveOutside, this);
			L.DomEvent.off(document, 'mouseup', this._onMouseUpOutside, this);
			L.DomEvent.off(this._map._resizeDetector.contentWindow, 'mousemove', this._onMouseMoveOutside, this);
			L.DomEvent.off(this._map._resizeDetector.contentWindow, 'mouseup', this._onMouseUpOutside, this);
		}
		else if (e.type === 'mousedown') {
			docLayer._resetPreFetching();
			this._mouseDown = true;
			if (this._holdMouseEvent) {
				this._stopQueue();
			}
			var mousePos = docLayer._latLngToTwips(e.latlng);
			this._queueMouseEvent(['buttondown', mousePos.x, mousePos.y, 1, buttons, modifier], 500);
		}
		else if (e.type === 'mouseup') {
			this._mouseDown = false;
			if (this._map.dragging.enabled()) {
				if (this._mouseEventsQueue.length === 0) {
					// mouse up after panning
					return;
				}
			}
			this._stopQueue();
			if (this._clickTime && Date.now() - this._clickTime <= 250) {
				// double click, a click was sent already
				this._mouseEventsQueue = [];
				this._clickCount++;
				if (this._clickCount < 4) {
					// Reset the timer in order to keep resetting until
					// we could have sent through a quadruple click. After this revert
					// to normal behaviour so that a following single-click is treated
					// as a separate click, in order to match LO desktop behaviour.
					// (Clicking five times results in paragraph selection after 4 clicks,
					// followed by resetting to a single cursor and no selection on the
					// fifth click.)
					this._clickTime = Date.now();
				}
				return;
			}
			else {
				this._clickTime = Date.now();
				this._clickCount = 1;
				mousePos = docLayer._latLngToTwips(e.latlng);
				var timeOut = 250;
				if (this._map._permission === 'edit') {
					timeOut = 0;
				}
				this._queueMouseEvent(['buttonup', mousePos.x, mousePos.y, 1, buttons, modifier]);
				this._holdMouseEvent = setTimeout(L.bind(this._executeMouseEvents, this), timeOut);

				for (key in docLayer._selectionHandles) {
					var handle = docLayer._selectionHandles[key];
					if (handle._icon) {
						L.DomUtil.removeClass(handle._icon, 'leaflet-not-clickable');
					}
				}
			}

			this._map.fire('scrollvelocity', {vx: 0, vy: 0});
		}
		else if (e.type === 'mousemove' && this._mouseDown) {
			if (this._holdMouseEvent) {
				if (this._map.dragging.enabled()) {
					// The user just panned the document
					this._mouseEventsQueue = [];
					return;
				}
				// synchronously execute old mouse events so we know that
				// they arrive to the server before the move command
				this._executeMouseEvents();
			}
			if (!this._map.dragging.enabled()) {
				mousePos = docLayer._latLngToTwips(e.latlng);
				this._queueMouseEvent(['move', mousePos.x, mousePos.y, 1, buttons, modifier]);

				for (key in docLayer._selectionHandles) {
					handle = docLayer._selectionHandles[key];
					if (handle._icon) {
						L.DomUtil.addClass(handle._icon, 'leaflet-not-clickable');
					}
				}

				this._map.fire('handleautoscroll', {pos: e.containerPoint, map: this._map});
			}
		}
		else if (e.type === 'mousemove' && !this._mouseDown) {
			mousePos = docLayer._latLngToTwips(e.latlng);
			this._queueMouseEvent(['move', mousePos.x, mousePos.y, 1, 0, modifier], 100);
		}
		else if (e.type === 'dblclick' || e.type === 'trplclick' || e.type === 'qdrplclick') {
			mousePos = docLayer._latLngToTwips(e.latlng);
			var clicks = {
				dblclick: 2,
				trplclick: 3,
				qdrplclick: 4
			};
			var count = clicks[e.type];

			docLayer._postMouseEvent('buttondown', mousePos.x, mousePos.y, count, buttons, modifier);
			docLayer._postMouseEvent('buttonup', mousePos.x, mousePos.y, count, buttons, modifier);
		}
		else if (mouseEnteringLeavingMap && e.type === 'mouseout' && this._mouseDown) {
			L.DomEvent.on(this._map._resizeDetector.contentWindow, 'mousemove', this._onMouseMoveOutside, this);
			L.DomEvent.on(this._map._resizeDetector.contentWindow, 'mouseup', this._onMouseUpOutside, this);
			L.DomEvent.on(document, 'mousemove', this._onMouseMoveOutside, this);
			L.DomEvent.on(document, 'mouseup', this._onMouseUpOutside, this);
		}
	},

	// Adds a mouse event message (an array) to the internal queue
	// If 'timeout' is zero, will trigger sending all events to lowsd immediately.
	// If 'timeout' is not zero, it will set a timeout to send all events.
	// If 'timeout' is not given, it will only push to the queue.
	_queueMouseEvent: function(me, timeout) {
		if (!this._holdMouseEvent && timeout) {
			this._holdMouseEvent = setTimeout(this._executeMouseEvents.bind(this), timeout);
		}
		this._mouseEventsQueue.push(me);
		if (!this._holdMouseEvent && timeout === 0) {
			this._executeMouseEvents();
		}
	},

	// Pauses the queue, preventing it from being sent to lowsd.
	_stopQueue: function() {
		clearTimeout(this._holdMouseEvent);
		this._holdMouseEvent = null;
	},

	// Sends all items from the queue to lowsd. Typically called from a timeout.
	_executeMouseEvents: function () {
		console.log('Flushing: ', this._mouseEventsQueue);

		this._stopQueue();

		// Mac-specific filter pass: Remove ctrl-right clicks
		// These trigger a 'contextmenu' event already, filtering
		// them out is needed to prevent double-handling. If this was not
		// being done, selecting a block of text and then ctrl-clicking
		// to display the context menu would de-select the text.
		if (navigator.platform === 'MacIntel') {
			if (this._mouseEventsQueue.some(function(item) {
				return item[0] === 'contextmenu'
			})) {
				this._mouseEventsQueue = this._mouseEventsQueue.filter(function(item) {
					return !(item[4] === this.LOButtons.left && item[5] === this._map.keyboard.keyModifier.ctrl);
				}.bind(this));
			}
		}

		for (var i = 0; i < this._mouseEventsQueue.length; i++) {
			var ev = this._mouseEventsQueue[i];

			if (ev[0] === 'buttondown' || ev[0] === 'buttonup' || ev[0] === 'move') {
				this._map._docLayer._postMouseEvent.apply(this._map._docLayer, ev);
			}

			if (ev[0] === 'buttonup') {
				this._map.focus();
			}

			if (ev[0] === 'contextmenu') {
				// Check if there are right-click events queued.
				if (!this._mouseEventsQueue.some(
					function(item) { return item[0] === 'buttondown' && item[4] === this.LOButtons.right }.bind(this)
				)) {
					// If not, then this is either a ctrl+click on a mac, a keypress
					// of the context menu key, a long touchscreen tap, or otherwise
					// a non-right-click way of triggering a context menu.
					// Unfortunately lowsd/lokit only understands right-clicks as triggers
					// for context menus, so this sends a synthetic right click message.
					this._map._docLayer._postMouseEvent('buttondown', ev[1], ev[2], 1, 4, 0);
					this._map._docLayer._postMouseEvent('buttonup', ev[1], ev[2], 1, 4, 0);
				}
			}
		}
		this._mouseEventsQueue = [];
	},

	_onMouseMoveOutside: function (e) {
		this._map._handleDOMEvent(e);
		if (this._map.dragging.enabled()) {
			this._map.dragging._draggable._onMove(e);
		}
	},

	_onMouseUpOutside: function (e) {
		this._mouseDown = false;
		L.DomEvent.off(document, 'mousemove', this._onMouseMoveOutside, this);
		L.DomEvent.off(document, 'mouseup', this._onMouseUpOutside, this);
		L.DomEvent.off(this._map._resizeDetector.contentWindow, 'mousemove', this._onMouseMoveOutside, this);
		L.DomEvent.off(this._map._resizeDetector.contentWindow, 'mouseup', this._onMouseUpOutside, this);

		this._map._handleDOMEvent(e);
		if (this._map.dragging.enabled()) {
			this._map.dragging._draggable._onUp(e);
		}
	}
});

L.Map.addInitHook('addHandler', 'mouse', L.Map.Mouse);
