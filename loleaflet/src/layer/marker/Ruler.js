/*
 * L.Annotation
 */

/* global $ L */

L.Annotation = L.Layer.extend({
	options: {
		interactive: true,
		margin1: null,
		margin2: null,
		nullOffset: null,
		pageOffset: null,
		pageWidth: null,
		unit: null,
		converPxToUnit: {
			'cm': 567
		},
		passiveMarker: [],
		activeMarker: []
	},

	initialize: function(map, options) {
		L.setOptions(this, options);
		this._map = map;
		this._map.on('zoomend', this._onRulerZoom, this);
		this._map.on('rulerdrag', this._onRulerDrag, this);
	},

	hideRuler: function() {
		$('.loleaflet-ruler').hide();
	},

	_initLayout: function() {
		var classMajorSep = 'loleaflet-ruler-maj',
		classMinorSep = 'loleaflet-ruler-min',
		leftMargin = _('Left Margin'),
		rightMargin = _('Right Margin');

		var rWrapper = this._rWrapper = L.DomUtil.create('div', 'loleaflet-ruler');
		var rContent = this._rContent = L.DomUtil.create('div', 'loleaflet-ruler-content', rWrapper);
		var rFace = this._rFace = L.DomUtil.create('div', 'loleaflet-ruler-face', rContent);
		_updateBreakPoints(rFace);

		L.DomEvent.on(rFace, click, this._onClicked, this);
		L.DomEvent.on(rFace, 'mousemove', this._onMouseMoved, this);
		L.DomEvent.on(rFace, 'mouseleave', this._onMouseLeave, this);
	},

	_updateBreakPoints: function() {
		if (margin1 == null || margin2 == null)
			return;

		$('.' + classMinorSep).remove();
		$('.' + classMajorSep).remove();

		var classMajorSep = 'loleaflet-ruler-maj',
		classMinorSep = 'loleaflet-ruler-min',
		classMargin = 'loleaflet-ruler-margin',
		classDraggable = 'loleaflet-ruler-drag',
		leftMarginStr = _('Left Margin'),
		rightMarginStr = _('Right Margin'),
		convertRatio = converPxToUnit['unit'],
		lMargin, rMargin;

		lMargin = this.options.nullOffset;
		rMargin = this.options.pageWidth - (this.options.nullOffset + this.options.margin2);

		for (var pos = 0; pos <= pageWidth; pos++) {
			var number = Math.round(Math.abs(pos - lMargin)/convertRatio);

			if (pos < lMargin) {
				if (this.options.passiveMarker[number])
					continue;

				var marker = L.DomUtil.create('div', classMajorSep, this.rFace);
				// add text or something if required
				this.options.passiveMarker[number] = marker;
			}
			else if (pos == lMargin) {
				if (this.options.interactive)
					var marker = L.DomUtil.create('div', classMargin + classDraggable, this.rFace);
				else
					var marker = L.DomUtil.create('div', classMargin, this.rFace);
			}
			else if (pos < (this.options.pageWidth - rMargin)) {
				if (this.options.activeMarker[number])
					continue;

				var marker = L.DomUtil.create('div', classMajorSep, this.rFace);
				this.options.activeMarker[number] = marker;
			}
			else {
				if (this.options.passiveMarker[number])
					continue;

				var marker = L.DomUtil.create('div', classMajorSep, this.rFace);
				this.options.passiveMarker[number] = marker;
			}
		}
	}
});