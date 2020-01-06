/* -*- js-indent-level: 8 -*- */
/*
 * L.Draggable allows you to add dragging capabilities to any element. Supports mobile devices too.
 */
var _timeStamp, _docPos, _velocityY, _velocityX, _ticker,  _amplitudeY, _amplitudeX,  _targetY;

L.Draggable = L.Evented.extend({

	statics: {
		START: L.Browser.touch ? ['touchstart', 'mousedown'] : ['mousedown'],
		END: {
			mousedown: 'mouseup',
			touchstart: 'touchend',
			pointerdown: 'touchend',
			MSPointerDown: 'touchend'
		},
		MOVE: {
			mousedown: 'mousemove',
			touchstart: 'touchmove',
			pointerdown: 'touchmove',
			MSPointerDown: 'touchmove'
		}
	},

	initialize: function (element, dragStartTarget, preventOutline) {
		this._element = element;
		this._dragStartTarget = dragStartTarget || element;
		this._preventOutline = preventOutline;
		this._freezeX = false;
		this._freezeY = false;
	},

	freezeX: function (boolChoice) {
		this._freezeX = boolChoice;
	},

	freezeY: function (boolChoice) {
		this._freezeY = boolChoice;
	},

	enable: function () {
		if (this._manualDrag || this._enabled) { return; }

		L.DomEvent.on(this._dragStartTarget, L.Draggable.START.join(' '), this._onDown, this);

		this._enabled = true;
	},

	disable: function () {
		if (this._manualDrag || !this._enabled) { return; }

		L.DomEvent.off(this._dragStartTarget, L.Draggable.START.join(' '), this._onDown, this);

		this._enabled = false;
		this._moved = false;
	},

	_velocityTracker: function() {
		var now, elepsed, delta, v, mapPanRect, offset;
		
		now = Date.now();
		elepsed = now - _timeStamp;
		_timeStamp = now;

		mapPanRect = document.getElementsByClassName('leaflet-pane leaflet-map-pane')[0].getBoundingClientRect();
		offset = new L.Point(mapPanRect.x, mapPanRect.y);
		delta = offset.subtract(_docPos);
		
		_docPos = offset;

		v = 1000 * delta.y / (1 + elepsed);
		_velocityY = 0.8 * v + 0.2 * _velocityY;

		v = 1000 * delta.x / (1 + elepsed);
		_velocityX = 0.8 * v + 0.2 * _velocityX;
	},

	_onDown: function (e) {
		this._moved = false;

		if (e.shiftKey || ((e.which !== 1) && (e.button !== 0) && !e.touches)) { return; } 

		// enable propagation of the mousedown event from map pane to parent elements in view mode
		// see bug bccu1446
		if (!L.DomUtil.hasClass(this._element, 'leaflet-map-pane')) {
			L.DomEvent.stopPropagation(e);
		}

		if (document.getElementsByClassName('leaflet-pane leaflet-map-pane')[0].contains(e.target)) {
			console.log('Pranam:Tap');
			_velocityX = _amplitudeX = _velocityY = _amplitudeY = 0;
			_timeStamp = Date.now();
			
			_amplitudeX = _velocityX;
			_velocityX = _amplitudeX;

			var mapPanRect = document.getElementsByClassName('leaflet-pane leaflet-map-pane')[0].getBoundingClientRect();
			_docPos = new L.Point(mapPanRect.x, mapPanRect.y);

			clearInterval(this._ticker);
			_ticker = setInterval(this._velocityTracker, 100);
			// return;
		}

		if (this._preventOutline) {
			L.DomUtil.preventOutline(this._element);
		}

		if (L.DomUtil.hasClass(this._element, 'leaflet-zoom-anim')) { return; }

		L.DomUtil.disableImageDrag();
		L.DomUtil.disableTextSelection();

		if (this._moving) { return; }

		this.fire('down');

		var first = e.touches ? e.touches[0] : e;

		this._startPoint = new L.Point(first.clientX, first.clientY);
		this._startPos = this._newPos = L.DomUtil.getPosition(this._element);
		var startBoundingRect = this._element.getBoundingClientRect();
		// Store offset between mouse selection position, and top left
		// We don't use this internally, but it is needed for external
		// manipulation of the cursor position, e.g. when adjusting
		// for scrolling during cursor dragging.
		this.startOffset = this._startPoint.subtract(new L.Point(startBoundingRect.left, startBoundingRect.top));

		if (!this._manualDrag) {
			L.DomEvent
				.on(document, L.Draggable.MOVE[e.type], this._onMove, this)
				.on(document, L.Draggable.END[e.type], this._onUp, this);
		}
	},

	_onMove: function (e) {
		if (!this._startPoint) {
			return;
		}

		if (e.touches && e.touches.length > 1) {
			this._moved = true;
			return;
		}

		if (document.getElementsByClassName('leaflet-pane leaflet-map-pane')[0].contains(e.target)) {
			console.log('Pranam:Move');
			// return;
		}

		var first = (e.touches && e.touches.length === 1 ? e.touches[0] : e),
		    newPoint = new L.Point(first.clientX, first.clientY),
		    offset = newPoint.subtract(this._startPoint);

		if (this._map) {
			// needed in order to avoid a jump when the document is dragged and the mouse pointer move
			// from over the map into the html document element area which is not covered by tiles
			// (resize-detector iframe)
			if (e.currentTarget && e.currentTarget.frameElement
				&& L.DomUtil.hasClass(e.currentTarget.frameElement, 'resize-detector')) {
				var rect = this._map._container.getBoundingClientRect(),
				    correction = new L.Point(rect.left, rect.top);
				offset = offset.add(correction);
			}
			if (this._map.getDocSize().x < this._map.getSize().x) {
				// don't pan horizontally when the document fits in the viewing
				// area horizontally (docWidth < viewAreaWidth)
				offset.x = 0;
			}
			if (this._map.getDocSize().y < this._map.getSize().y) {
				// don't pan vertically when the document fits in the viewing
				// area horizontally (docHeight < viewAreaHeight)
				offset.y = 0;
			}
		}
		if (!offset.x && !offset.y) { return; }
		if (L.Browser.touch && Math.abs(offset.x) + Math.abs(offset.y) < 3) { return; }

		L.DomEvent.preventDefault(e);

		if (!this._moved) {
			this.fire('dragstart', {originalEvent: e});

			this._moved = true;
			this._startPos = L.DomUtil.getPosition(this._element).subtract(offset);

			L.DomUtil.addClass(document.body, 'leaflet-dragging');

			this._lastTarget = e.target || e.srcElement;
			L.DomUtil.addClass(this._lastTarget, 'leaflet-drag-target');
		}

		this._newPos = this._startPos.add(offset);

		if (this._freezeY)
			this._newPos.y = this._startPos.y
		if (this._freezeX)
			this._newPos.x = this._startPos.x

		this._moving = true;

		L.Util.cancelAnimFrame(this._animRequest);
		this._lastEvent = e;

		this._animRequest = L.Util.requestAnimFrame(this._updatePosition, this, true, this._dragStartTarget);
	},

	_updatePosition: function () {
		var e = {originalEvent: this._lastEvent};
		this.fire('predrag', e);
		L.DomUtil.setPosition(this._element, this._newPos);
		this.fire('drag', e);
	},

	_autoScroll: function() {
		console.log('Pranam: autoscroll');
		var elepsed, delta, timeconstan = 325;
	
		if (_amplitudeY) {
			elepsed = Date.now() - _timeStamp;
			delta = -_amplitudeY * Math.exp(-elepsed / timeconstan);
			if (delta > 0.5 || delta < -0.5) {
				L.DomUtil.setPosition(document.getElementsByClassName('leaflet-pane leaflet-map-pane')[0], _targetY + delta);
				requestAnimationFrame(this._autoScroll);
			} else {
				L.DomUtil.setPosition(document.getElementsByClassName('leaflet-pane leaflet-map-pane')[0], _targetY);
			}
		}
	
	},

	_onUp: function (e) {
		L.DomUtil.removeClass(document.body, 'leaflet-dragging');

		if (this._lastTarget) {
			L.DomUtil.removeClass(this._lastTarget, 'leaflet-drag-target');
			this._lastTarget = null;
		}
		if (document.getElementsByClassName('leaflet-pane leaflet-map-pane')[0].contains(e.target)) {
			console.log('Pranam:Up');
			clearInterval(_ticker);

			if (_velocityY > 10 || _velocityY < 10) {
				_amplitudeY = 0.8 * _velocityY;
				_targetY = Math.round(_docPos.y + _amplitudeY);
				_timeStamp = Date.now();
				requestAnimationFrame(this._autoScroll);
			}
			return;
		} 

		for (var i in L.Draggable.MOVE) {
			L.DomEvent
			    .off(document, L.Draggable.MOVE[i], this._onMove, this)
			    .off(document, L.Draggable.END[i], this._onUp, this);
		}

		L.DomUtil.enableImageDrag();
		L.DomUtil.enableTextSelection();

		if (this._moved && this._moving) {
			// ensure drag is not fired after dragend
			L.Util.cancelAnimFrame(this._animRequest);

			this.fire('dragend', {
				originalEvent: e,
				distance: this._newPos.distanceTo(this._startPos)
			});
		} else {
			this.fire('up', {originalEvent: e});
		}

		this._moving = false;
		this._startPoint = undefined;
	}
});
