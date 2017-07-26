/*
 * L.Annotation
 */

/* global $ L */
L.Control.Ruler = L.Control.extend({
	options: {
		interactive: true,
		margin1: null,
		margin2: null,
		nullOffset: null,
		pageOffset: null,
		pageWidth: null,
		unit: null,
		convertPxToUnit: {
			'cm': 567
		},
		passiveMarker: [],
		activeMarker: []
	},

	hideRuler: function() {
		$('.loleaflet-ruler').hide();
	},

	onAdd: function(map) {
		console.log('onAdd');
		return this._initLayout();
	},

	_initLayout: function() {
		console.log('initialize layout');
		var classMajorSep = 'loleaflet-ruler-maj',
		classMinorSep = 'loleaflet-ruler-min',
		leftMargin = _('Left Margin'),
		rightMargin = _('Right Margin');

		var rWrapper = this._rWrapper = L.DomUtil.create('div', 'loleaflet-ruler leaflet-bar leaflet-control leaflet-control-custom');
		var rContent = this._rContent = L.DomUtil.create('div', 'loleaflet-ruler-content', rWrapper);
		var rFace = this._rFace = L.DomUtil.create('div', 'loleaflet-ruler-face', rContent);
		this._rWrapper.style.backgroundColor = 'black';
		this._rWrapper.style.width = '30px';
		this._rWrapper.style.height = '30px';
		this._rFace.innerText = 'ruler';
		this._updateBreakPoints();

		return this._rWrapper;

		// return this._rWrapper;
		// L.DomEvent.on(rFace, 'click', this._onClicked, this);
		// L.DomEvent.on(rFace, 'mousemove', this._onMouseMoved, this);
	},

	_onClicked: function() {

	},

	_onMouseMoved: function() {

	},

	_updateBreakPoints: function() {
		// if (this.options.margin1 == null || this.options.margin2 == null)
		// 	return;

		$('.' + classMinorSep).remove();
		$('.' + classMajorSep).remove();

		var classMajorSep = 'loleaflet-ruler-maj',
		classMinorSep = 'loleaflet-ruler-min',
		classMargin = 'loleaflet-ruler-margin',
		classDraggable = 'loleaflet-ruler-drag',
		leftMarginStr = _('Left Margin'),
		rightMarginStr = _('Right Margin'),
		convertRatio = this.options.convertPxToUnit['unit'],
		lMargin, rMargin;

		lMargin = this.options.nullOffset;
		rMargin = this.options.pageWidth - (this.options.nullOffset + this.options.margin2);


		// for (var pos = 0; pos <= this.options.pageWidth; pos++) {

		// 	var number = Math.round(Math.abs(pos - lMargin)/convertRatio);

		// 	if (pos < lMargin) {
		// 		if (this.options.passiveMarker[number])
		// 			continue;

		// 		var marker = L.DomUtil.create('div', classMajorSep, this._rFace);
		// 		// add text or something if required
		// 		this.options.passiveMarker[number] = marker;
		// 	}
		// 	else if (pos == lMargin) {
		// 		if (this.options.interactive)
		// 			var marker = L.DomUtil.create('div', classMargin + classDraggable, this.rFace);
		// 		else
		// 			var marker = L.DomUtil.create('div', classMargin, this._rFace);
		// 	}
		// 	else if (pos < (this.options.pageWidth - rMargin)) {
		// 		if (this.options.activeMarker[number])
		// 			continue;

		// 		var marker = L.DomUtil.create('div', classMajorSep, this._rFace);
		// 		this.options.activeMarker[number] = marker;
		// 	}
		// 	else {
		// 		if (this.options.passiveMarker[number])
		// 			continue;

		// 		var marker = L.DomUtil.create('div', classMajorSep, this._rFace);
		// 		this.options.passiveMarker[number] = marker;
		// 	}
		// }
	}
});


L.control.ruler = function (options) {
	return new L.Control.Ruler(options);
};