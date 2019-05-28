/*
 * A Leaflet layer that shows dark overlay around focused object.
 *
 */
/* global $ */

L.ObjectFocusDarkOverlay = L.Layer.extend({
	onAdd: function() {
	},

	remove: function() {
	},
});

// Libreoffice-specific functionality follows.

/*
 * A L.ObjectFocusDarkOverlay
 */
L.ObjectFocusDarkOverlay = L.ObjectFocusDarkOverlay.extend({
	onAdd: function(map) {
		map.on('inplace', this._onStateChanged, this);
	},

	remove: function() {
		delete this._map.style.background;
		this._map.off('inplace', this._onStateChanged, this);
	},

	_createPart: function(id, x, y, w, h) {
		var element = document.createElement('img');
		element.id = id;
		$('.leaflet-tile-container').append(element);

		$('#' + id).css('background-color', 'black');
		$('#' + id).css('position', 'absolute');
		$('#' + id).css('z-index', '50');
		$('#' + id).css('top', y);
		$('#' + id).css('left', x);
		$('#' + id).css('width', w);
		$('#' + id).css('height', h);
		$('#' + id).css('opacity', '0.4');
	},

	_onStateChanged: function(args) {
		if (args.off && args.off === true) {
			$('#dark-top').remove();
			$('#dark-left').remove();
			$('#dark-right').remove();
			$('#dark-bottom').remove();
			return;
		}

		var fullWidth = 1000000;
		var fullHeight = 1000000;

		this._createPart('dark-top', 0, 0, fullWidth + 'px', (args.y + 1) + 'px');
		this._createPart('dark-left', 0, args.y + 'px', args.x + 'px', (args.h + 1) + 'px');
		this._createPart('dark-right', args.x + args.w + 'px', args.y + 'px', fullWidth + 'px', (args.h + 1) + 'px');
		this._createPart('dark-bottom', 0, (args.y + args.h) + 'px', fullWidth + 'px', fullHeight + 'px');
	}
});
