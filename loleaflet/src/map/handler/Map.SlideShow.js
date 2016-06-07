/*
 * L.Map.SlideShow is handling the slideShow action
 */

L.Map.mergeOptions({
	slideShow: true
});

L.Map.SlideShow = L.Handler.extend({

	initialize: function (map) {
		this._map = map;
	},

	addHooks: function () {
		this._map.on('fullscreen', this._onFullScreen, this);
		this._map.on('slidedownloadready', this._onSlideDownloadReady, this);
	},

	removeHooks: function () {
		this._map.off('fullscreen', this._onFullScreen, this);
		this._map.off('slidedownloadready', this._onSlideDownloadReady, this);
	},

	_onFullScreen: function () {
		this._slideShow = L.DomUtil.create('iframe', 'leaflet-slideshow', this._map._container);
		this._slideShow.src = this._map.options.webserver + '/' + this._map.options.urlPrefix + '/loleaflet/dist/loading.html';
		if (this._slideShow.requestFullscreen) {
			this._slideShow.requestFullscreen();
		}
		else if (this._slideShow.msRequestFullscreen) {
			this._slideShow.msRequestFullscreen();
		}
		else if (this._slideShow.mozRequestFullScreen) {
			this._slideShow.mozRequestFullScreen();
		}
		else if (this._slideShow.webkitRequestFullscreen) {
			this._slideShow.webkitRequestFullscreen();
		}

		L.DomEvent.on(document, 'fullscreenchange webkitfullscreenchange mozfullscreenchange msfullscreenchange',
				this._onFullScreenChange, this);

		this.fullscreen = true;
		this._map.downloadAs('slideshow.svg', 'svg', null, 'slideshow');
	},

	_onFullScreenChange: function () {

		this.fullscreen = document.fullscreen ||
			document.webkitIsFullScreen ||
			document.mozFullScreen ||
			document.msFullscreenElement;
		if (!this.fullscreen) {
			L.DomUtil.remove(this._slideShow);
		}
	},

	_onSlideDownloadReady: function (e) {
		this._slideShow.src = e.url + '?mime_type=image/svg%2Bxml';
		this._slideShow.contentWindow.focus();
		clearInterval(this._slideShow.contentWindow.spinnerInterval);
	}
});

L.Map.addInitHook('addHandler', 'slideShow', L.Map.SlideShow);
