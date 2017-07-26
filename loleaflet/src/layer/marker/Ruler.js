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
		map.on('docsize', this._updateBreakPoints, this);
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
		convertRatioDrag , lMargin, rMargin, wPixel, hPixel;

		lMargin = this.options.nullOffset;
		rMargin = this.options.pageWidth - (this.options.nullOffset + this.options.margin2);

		// Multiplication with this facor is temporary,
		// I think, we need to find the margin in the left tiles
		// and subtract here accordingly
		wPixel = .958*this._map._docLayer._docPixelSize.x;
		hPixel = this._map._docLayer._docPixelSize.y;

		convertRatioDrag = this.options.convertRatioDrag = wPixel / this.options.pageWidth;

		this._rFace.style.width = wPixel + 'px';
		this._rFace.style.backgroundColor = 'white';

		// for (var pos = 0; pos <= this.options.pageWidth; pos++) {

		// 	var number = Math.round(Math.abs(pos - lMargin)/convertRatio);

		// 	// if (pos < lMargin) {
		// 	// 	if (this.options.passiveMarker[number])
		// 	// 		continue;

		// 	// 	console.log(1, pos, number);
		// 	// 	var marker = L.DomUtil.create('div', classMajorSep, this._rFace);
		// 	// 	marker.style.width = wPixel/21 + 'px';
		// 	// 	this.options.passiveMarker[number] = marker;
		// 	// }
		// 	// else if (pos < (this.options.pageWidth - rMargin)) {
		// 	// 	if (this.options.activeMarker[number])
		// 	// 		continue;

		// 	// 	console.log(2, pos, number);
		// 	// 	var marker = L.DomUtil.create('div', classMajorSep, this._rFace);
		// 	// 	marker.style.width = wPixel/21 + 'px';
		// 	// 	this.options.activeMarker[number] = marker;
		// 	// }
		// 	// else {
		// 	// 	if (this.options.passiveMarker[number])
		// 	// 		continue;

		// 	// 	console.log(3, pos, number);
		// 	// 	var marker = L.DomUtil.create('div', classMajorSep, this._rFace);
		// 	// 	marker.style.width = wPixel/21 + 'px';
		// 	// 	this.options.passiveMarker[number] = marker;
		// 	// }
		// }

		if (!this.options.marginSet) {

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
	},

	_initiateDrag: function(e) {
		L.DomEvent.on(this._rFace, 'mousemove', this._moveMargin, this);
		L.DomEvent.on(this._map, 'mouseup', this._endDrag, this);
		L.DomUtil.addClass(this._rMarginDrag, 'leaflet-drag-moving');

		this._rFace.style.cursor = 'w-resize';
		this._initialposition = e.clientX;
	},

	_moveMargin: function(e) {
		var posChange = e.clientX - this._initialposition;
		var rMargin = this.options.pageWidth - (this.options.nullOffset + this.options.margin2);
		this._rMarginDrag.style.width = this.options.convertRatioDrag*rMargin - posChange + 'px';
	},

	_endDrag: function(e) {
		this._rFace.style.cursor = 'default';

		L.DomEvent.off(this._rFace, 'mousemove', this._moveMargin, this);
		L.DomUtil.removeClass(this._rMarginDrag, 'leaflet-drag-moving');

		var posChange = e.originalEvent.clientX - this._initialposition;
		var unoObj = {};
		unoObj['Margin2'] = {};
		unoObj['Margin2']['type'] = 'string';
		unoObj['Margin2']['value'] = posChange/this.options.convertRatioDrag;

		this._map._socket.sendMessage('uno .uno:RulerChangeState ' + JSON.stringify(unoObj));
	}
});


L.control.ruler = function (options) {
	return new L.Control.Ruler(options);
};