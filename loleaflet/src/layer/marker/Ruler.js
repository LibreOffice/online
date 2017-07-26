/*
 * L.Annotation
 */

/* global $ L */
L.Control.Ruler = L.Control.extend({
	options: {
		interactive: true,
		marginSet: false,
		margin1: 0,
		margin2: 9972,
		nullOffset: 1134,
		pageOffset: 284,
		pageWidth: 12240,
		unitWidth: 210,
		unitHeight: 297,
		landscape: false,
		unit: null,
		convertPxToUnit: {
			'cm': 567
		},
		convertRatioDrag: null,
		passiveMarker: [],
		activeMarker: []
	},

	hideRuler: function() {
		$('.loleaflet-ruler').hide();
	},

	onAdd: function(map) {
		console.log('onAdd');
		map.on('rulerupdate', this._updateOptions, this);
		map.on('docsize', this._updateBreakPoints, this);
		map.on('pageSizeChange', this._updatePageSize, this);
		return this._initLayout();
	},

	_initLayout: function() {
		console.log('initialize layout');

		this._rWrapper = L.DomUtil.create('div', 'loleaflet-ruler leaflet-bar leaflet-control leaflet-control-custom');
		this._rContent = L.DomUtil.create('div', 'loleaflet-ruler-content', this._rWrapper);
		this._rFace = L.DomUtil.create('div', 'loleaflet-ruler-face', this._rContent);

		return this._rWrapper;
	},

	_onClicked: function() {

	},

	_onMouseMoved: function() {

	},

	_updateOptions: function(obj) {
		this.options.margin1 = parseInt(obj['margin1']);
		this.options.margin2 = parseInt(obj['margin2']);
		this.options.nullOffset = parseInt(obj['leftOffset']);
		this.options.pageWidth = parseInt(obj['pageWidth']);
		this.options.unit = obj['unit'].trim();

		this._updateBreakPoints();
	},

	_updatePageSize: function(obj) {
		this.options.unitWidth = obj['width'];
		this.options.unitHeight = obj['height'];
		this.options.landscape = obj['landscape'];

		this._updateBreakPoints();
	},

	_updateBreakPoints: function() {
		if (this.options.margin1 == null || this.options.margin2 == null)
			return;

		$('.' + classMinorSep).remove();
		$('.' + classMajorSep).remove();

		var classMajorSep = 'loleaflet-ruler-maj',
		classMinorSep = 'loleaflet-ruler-min',
		classMargin = 'loleaflet-ruler-margin',
		classDraggable = 'loleaflet-ruler-drag',
		rightComp = 'loleaflet-ruler-right',
		leftComp = 'loleaflet-ruler-left',
		leftMarginStr = _('Left Margin'),
		rightMarginStr = _('Right Margin'),
		convertRatioDrag, convertRatioUnit , lMargin, rMargin, wPixel, hPixel;

		lMargin = this.options.nullOffset;
		rMargin = this.options.pageWidth - (this.options.nullOffset + this.options.margin2);

		// Multiplication with this facor is temporary,
		// I think, we need to find the margin in the left tiles
		// and subtract here accordingly
		wPixel = .958*this._map._docLayer._docPixelSize.x;
		hPixel = this._map._docLayer._docPixelSize.y;

		convertRatioDrag = this.options.convertRatioDrag = wPixel / this.options.pageWidth;
		convertRatioUnit = wPixel / this.options.unitWidth;

		this._rFace.style.width = wPixel + 'px';
		this._rFace.style.backgroundColor = 'white';

		if (!this.options.marginSet) {

			// for (var pos = 0; pos <= this.options.unitWidth; pos+=5) {

			// 	var number = Math.round(Math.abs(pos - lMargin)/10);

			// 	if (pos < lMargin) {
			// 		if (this.options.passiveMarker[number])
			// 			continue;

			// 		console.log(1, pos, number);
			// 		var marker = L.DomUtil.create('div', classMajorSep, this._rFace);
			// 		marker.style.width = convertRatioUnit*10 + 'px';
			// 		this.options.passiveMarker[number] = marker;
			// 	}
			// 	else if (pos < (this.options.pageWidth - rMargin)) {
			// 		if (this.options.activeMarker[number])
			// 			continue;

			// 		console.log(2, pos, number);
			// 		var marker = L.DomUtil.create('div', classMajorSep, this._rFace);
			// 		marker.style.width = convertRatioUnit*10 + 'px';
			// 		this.options.activeMarker[number] = marker;
			// 	}
			// 	else {
			// 		if (this.options.passiveMarker[number])
			// 			continue;

			// 		console.log(3, pos, number);
			// 		var marker = L.DomUtil.create('div', classMajorSep, this._rFace);
			// 		marker.style.width = convertRatioUnit*10 + 'px';
			// 		this.options.passiveMarker[number] = marker;
			// 	}
			// }

			this.options.marginSet = true;

			this._lMarginMarker = L.DomUtil.create('div', classMargin + ' ' + leftComp, this._rFace);
			this._rMarginMarker =  L.DomUtil.create('div', classMargin + ' ' + rightComp, this._rFace);

			if (this.options.interactive) {
				this._lMarginDrag = L.DomUtil.create('div', classDraggable + ' ' + leftComp, this._lMarginMarker);
				this._rMarginDrag = L.DomUtil.create('div', classDraggable + ' ' + rightComp, this._rMarginMarker);
				this._lMarginDrag.style.cursor = 'e-resize';
				this._rMarginDrag.style.cursor = 'w-resize';
				this._lMarginDrag.title = leftMarginStr;
				this._rMarginDrag.title = rightMarginStr;
			}

		}

		this._lMarginMarker.style.width = (convertRatioDrag*lMargin) + 'px';
		this._rMarginMarker.style.width = (convertRatioDrag*rMargin) + 'px';

		if (this.options.interactive) {
			this._lMarginDrag.style.width = (convertRatioDrag*lMargin) + 'px';
			this._rMarginDrag.style.width = (convertRatioDrag*rMargin) + 'px';
		}

		L.DomEvent.on(this._rMarginDrag, 'mousedown', this._initiateDrag, this);
		L.DomEvent.on(this._lMarginDrag, 'mousedown', this._initiateDrag, this);
	},

	_initiateDrag: function(e) {
		L.DomEvent.on(this._rFace, 'mousemove', this._moveMargin, this);
		L.DomEvent.on(this._map, 'mouseup', this._endDrag, this);
		this._initialposition = e.clientX;

		if (L.DomUtil.hasClass(e.srcElement, 'loleaflet-ruler-right')) {
			L.DomUtil.addClass(this._rMarginDrag, 'leaflet-drag-moving');
			this._rFace.style.cursor = 'w-resize';
		}
		else {
			L.DomUtil.addClass(this._lMarginDrag, 'leaflet-drag-moving');
			this._rFace.style.cursor = 'e-resize';
		}
	},

	_moveMargin: function(e) {
		var posChange = e.clientX - this._initialposition;

		if (L.DomUtil.hasClass(this._rMarginDrag, 'leaflet-drag-moving')) {
			var rMargin = this.options.pageWidth - (this.options.nullOffset + this.options.margin2);
			this._rMarginDrag.style.width = this.options.convertRatioDrag*rMargin - posChange + 'px';
		}
		else {
			this._lMarginDrag.style.width = this.options.convertRatioDrag*this.options.nullOffset + posChange + 'px';
		}
	},

	_endDrag: function(e) {
		var posChange = e.originalEvent.clientX - this._initialposition;
		var unoObj = {}, marginType;

		L.DomEvent.off(this._rFace, 'mousemove', this._moveMargin, this);
		L.DomEvent.off(this._map, 'mouseup', this._endDrag, this);

		if (L.DomUtil.hasClass(e.originalEvent.srcElement, 'loleaflet-ruler-right')) {
			marginType = 'Margin2';
			L.DomUtil.removeClass(this._rMarginDrag, 'leaflet-drag-moving');
		}
		else {
			marginType = 'Margin1';
			L.DomUtil.removeClass(this._lMarginDrag, 'leaflet-drag-moving');
		}

		this._rFace.style.cursor = 'default';

		unoObj[marginType] = {};
		unoObj[marginType]['type'] = 'string';
		unoObj[marginType]['value'] = posChange/this.options.convertRatioDrag;
		this._map._socket.sendMessage('uno .uno:RulerChangeState ' + JSON.stringify(unoObj));
	}
});

L.control.ruler = function (options) {
	return new L.Control.Ruler(options);
};