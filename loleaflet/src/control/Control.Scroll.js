/* -*- js-indent-level: 8 -*- */
/*
 * L.Control.Scroll handles scrollbars
 */

/* global $ clearTimeout setTimeout */
L.Control.Scroll = L.Control.extend({

	onAdd: function (map) {
		this._scrollContainer = L.DomUtil.create('div', 'scroll-container', map._container.parentElement);
		this._mockDoc = L.DomUtil.create('div', '', this._scrollContainer);
		this._mockDoc.id = 'mock-doc';

		this._prevScrollX = 0;
		this._prevScrollY = 0;

		this._prevDocWidth = 0;
		this._prevDocHeight = 0;

		map.on('scrollto', this._onScrollTo, this);
		map.on('scrollby', this._onScrollBy, this);
		map.on('scrollvelocity', this._onScrollVelocity, this);
		map.on('handleautoscroll', this._onHandleAutoScroll, this);
		map.on('docsize', this._onUpdateSize, this);
		map.on('updatescrolloffset', this._onUpdateScrollOffset, this);
		map.on('updaterowcolumnheaders', this._onUpdateRowColumnHeaders, this);

		var control = this;
		var autoHideTimeout = null;
		$('.scroll-container').mCustomScrollbar({
			axis: 'yx',
			theme: 'minimal-dark',
			scrollInertia: 0,
			advanced:{
				autoExpandHorizontalScroll: true, /* weird bug, it should be false */
				jumpOnContentResize: false /* prevent from jumping on a mobile devices */
			},
			callbacks:{
				onScrollStart: function() {
					control._map.fire('closepopup');
				},
				onScroll: function() {
					control._onScrollEnd(this);
					if (autoHideTimeout)
						clearTimeout(autoHideTimeout);
					autoHideTimeout = setTimeout(function() {
						//	$('.mCS-autoHide > .mCustomScrollBox ~ .mCSB_scrollTools').css({opacity: 0, 'filter': 'alpha(opacity=0)', '-ms-filter': 'alpha(opacity=0)'});
						$('.mCS-autoHide > .mCustomScrollBox ~ .mCSB_scrollTools').removeClass('loleaflet-scrollbar-show');
					}, 2000);
				},
				whileScrolling: function() {
					control._onScroll(this);

					// autoHide feature doesn't work because plugin relies on hovering on scroll container
					// and we have a mock scroll container whereas the actual user hovering happens only on
					// real document. Change the CSS rules manually to simulate autoHide feature.
					$('.mCS-autoHide > .mCustomScrollBox ~ .mCSB_scrollTools').addClass('loleaflet-scrollbar-show');
				},
				onUpdate: function() {
					console.debug('mCustomScrollbar: onUpdate:');
				},
				alwaysTriggerOffsets: false
			}
		});
	},

	_onCalcScroll: function (e) {
		if (!this._map._enabled) {
			return;
		}

		var newLeft = -e.mcs.left;
		if (newLeft > this._prevScrollX) {
			var viewportWidth = this._map.getSize().x;
			var docWidth = this._map._docLayer._docPixelSize.x;
			newLeft = Math.min(newLeft, docWidth - viewportWidth);
		}
		else {
			newLeft = Math.max(newLeft, 0);
		}

		var newTop = -e.mcs.top;
		if (newTop > this._prevScrollY) {
			var viewportHeight = this._map.getSize().y;
			var docHeight = Math.round(this._map._docLayer._docPixelSize.y);
			newTop = Math.min(newTop, docHeight - viewportHeight);
		}
		else {
			newTop = Math.max(newTop, 0);
		}

		var offset = new L.Point(
				newLeft - this._prevScrollX,
				newTop - this._prevScrollY);

		if (offset.equals(new L.Point(0, 0))) {
			return;
		}

		this._onUpdateRowColumnHeaders({ x: newLeft, y: newTop, offset: offset});

		this._prevScrollY = newTop;
		this._prevScrollX = newLeft;
		this._map.fire('scrolloffset', offset);
		this._map.scroll(offset.x, offset.y);
	},

	_onScroll: function (e) {
		if (this._map._docLayer._docType === 'spreadsheet') {
			this._onCalcScroll(e);
			return;
		}

		console.debug('_onScroll: ');
		if (!this._map._enabled) {
			return;
		}

		if (this._ignoreScroll) {
			console.debug('_onScroll: ignoring scroll');
			return;
		}

		var offset = new L.Point(
			-e.mcs.left - this._prevScrollX,
			-e.mcs.top - this._prevScrollY);

		if (!offset.equals(new L.Point(0, 0))) {
			this._prevScrollY = -e.mcs.top;
			this._prevScrollX = -e.mcs.left;
			console.debug('_onScroll: scrolling: ' + offset);
			this._map.scroll(offset.x, offset.y);
			this._map.fire('scrolloffset', offset);
		}
	},

	_onScrollEnd: function (e) {
		// needed in order to keep the row/column header correctly aligned
		if (this._map._docLayer._docType === 'spreadsheet') {
			return;
		}

		console.debug('_onScrollEnd:');
		if (this._ignoreScroll) {
			this._ignoreScroll = null;
			console.debug('_onScrollEnd: scrollTop: ' + -e.mcs.top);
			this._map.scrollTop(-e.mcs.top);
		}
		this._prevScrollY = -e.mcs.top;
		this._prevScrollX = -e.mcs.left;
		// Scrolling quickly via mousewheel messes up the annotations for some reason
		// Triggering the layouting algorithm here, though unnecessary, fixes the problem.
		// This is just a workaround till we find the root cause of why it messes up the annotations
		if (this._map._docLayer._annotations.layout) {
			this._map._docLayer._annotations.layout();
		}
	},

	_onScrollTo: function (e) {
		// triggered by the document (e.g. search result out of the viewing area)
		$('.scroll-container').mCustomScrollbar('scrollTo', [e.y, e.x], {calledFromInvalidateCursorMsg: e.calledFromInvalidateCursorMsg});
	},

	_onScrollBy: function (e) {
		e.y *= (-1);
		var y = '+=' + e.y;
		if (e.y < 0) {
			y = '-=' + Math.abs(e.y);
		}
		e.x *= (-1);
		var x = '+=' + e.x;
		if (e.x < 0) {
			x = '-=' + Math.abs(e.x);
		}
		// Note: timeout===1 is checked in my extremely ugly hack in jquery.mCustomScrollbar.js.
		$('.scroll-container').mCustomScrollbar('scrollTo', [y, x], { timeout: 1 });
	},

	_onScrollVelocity: function (e) {
		if (e.vx === 0 && e.vy === 0) {
			clearInterval(this._autoScrollTimer);
			this._autoScrollTimer = null;
			this._map.isAutoScrolling = false;
		} else {
			clearInterval(this._autoScrollTimer);
			this._map.isAutoScrolling = true;
			this._autoScrollTimer = setInterval(L.bind(function() {
				this._onScrollBy({x: e.vx, y: e.vy});
			}, this), 100);
		}
	},

	_onHandleAutoScroll: function (e) {
		var vx = 0;
		var vy = 0;

		if (e.pos.y > e.map._size.y - 50) {
			vy = 50;
		} else if (e.pos.y < 50) {
			vy = -50;
		}
		if (e.pos.x > e.map._size.x - 50) {
			vx = 50;
		} else if (e.pos.x < 50) {
			vx = -50;
		}

		this._onScrollVelocity({vx: vx, vy: vy});
	},

	_onUpdateSize: function (e) {
		if (!this._mockDoc) {
			return;
		}

		// we need to avoid precision issues in comparison (in the end values are pixels)
		var newDocWidth = Math.ceil(e.x);
		var newDocHeight = Math.ceil(e.y);

		// for writer documents, ignore scroll while document size is being reduced
		if (this._map.getDocType() === 'text' && newDocHeight < this._prevDocHeight) {
			console.debug('_onUpdateSize: Ignore the scroll !');
			this._ignoreScroll = true;
		}

		// Use the rounded pixel values as it makes little sense to use fractional pixels.
		L.DomUtil.setStyle(this._mockDoc, 'width', newDocWidth + 'px');
		L.DomUtil.setStyle(this._mockDoc, 'height', newDocHeight + 'px');

		// custom scrollbar plugin checks automatically for content height changes but not for content width changes
		// so we need to update scrollbars explicitly; moreover we want to avoid to have 'update' invoked twice
		// in case prevDocHeight !== newDocHeight
		if (this._prevDocWidth !== newDocWidth && this._prevDocHeight === newDocHeight) {
			$('.scroll-container').mCustomScrollbar('update');
		}

		// Don't get them through L.DomUtil.getStyle because precision is no more than 6 digits
		this._prevDocWidth = newDocWidth;
		this._prevDocHeight = newDocHeight;
	},

	_onUpdateScrollOffset: function (e) {
		// used on window resize
		// also when dragging
		var offset = new L.Point(e.x - this._prevScrollX, e.y - this._prevScrollY);
		if (offset.x === 0) {
			offset.x = 1;
		}
		if (offset.y === 0) {
			offset.y = 1;
		}
		if (e.updateHeaders && this._map._docLayer._docType === 'spreadsheet') {
			this._onUpdateRowColumnHeaders({x: e.x, y: e.y, offset: offset});
		}
		this._map.fire('scrolloffset', offset);
		this._ignoreScroll = null;
		$('.scroll-container').mCustomScrollbar('stop');
		this._prevScrollY = e.y;
		this._prevScrollX = e.x;
		$('.scroll-container').mCustomScrollbar('scrollTo', [e.y, e.x], {callbacks: false, timeout:0});
	},

	_onUpdateRowColumnHeaders: function(e) {
		var offset = e.offset || {};

		var topLeftPoint = new L.Point(e.x, e.y);
		var sizePx = this._map.getSize();

		if (topLeftPoint.x === undefined) {
			topLeftPoint.x = this._map._getTopLeftPoint().x;
		}
		if (topLeftPoint.y === undefined) {
			topLeftPoint.y = this._map._getTopLeftPoint().y;
		}

		if (offset.x === 0) {
			topLeftPoint.x = -1;
			sizePx.x = 0;
		}
		if (offset.y === 0) {
			topLeftPoint.y = -1;
			sizePx.y = 0;
		}

		var pos = this._map._docLayer._pixelsToTwips(topLeftPoint);
		var size = this._map._docLayer._pixelsToTwips(sizePx);
		var payload = 'commandvalues command=.uno:ViewRowColumnHeaders?x=' + Math.round(pos.x) + '&y=' + Math.round(pos.y) +
			'&width=' + Math.round(size.x) + '&height=' + Math.round(size.y);

		if (e.outline) {
			payload += '&columnOutline=' + e.outline.column + '&groupLevel=' + e.outline.level
				+ '&groupIndex=' + e.outline.index + '&groupHidden=' + e.outline.hidden;
		}

		this._map._socket.sendMessage(payload);
	}
});

L.control.scroll = function (options) {
	return new L.Control.Scroll(options);
};
