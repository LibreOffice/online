/* -*- js-indent-level: 8 -*- */
/*
 * L.Control.PartsPreview
 */

/* global $ Hammer */
L.Control.PartsPreview = L.Control.extend({
	options: {
		autoUpdate: true
	},
	partsFocused: false,

	onAdd: function (map) {
		this._previewInitialized = false;
		this._previewTiles = [];
		this._partsPreviewCont = L.DomUtil.get('slide-sorter');
		this._scrollY = 0;
		this.isMobileMode = window.matchMedia('(max-width: 767px)').matches;//mobile mode means the slide selector is full screen and can be opened/closed with swipes

		map.on('updateparts', this._updateDisabled, this);
		map.on('updatepart', this._updatePart, this);
		map.on('tilepreview', this._updatePreview, this);
		map.on('insertpage', this._insertPreview, this);
		map.on('deletepage', this._deletePreview, this);
	},

	_updateDisabled: function (e) {
		var parts = e.parts;
		var selectedPart = e.selectedPart;
		var selectedParts = e.selectedParts;
		var docType = e.docType;
		if (docType === 'text') {
			return;
		}

		if (docType === 'presentation' || docType === 'drawing') {
			if (!this._previewInitialized)
			{
				// make room for the preview
				var control = this;
				var docContainer = this._map.options.documentContainer;
				L.DomUtil.addClass(docContainer, 'parts-preview-document');
				setTimeout(L.bind(function () {
					this._map.invalidateSize();
					$('.scroll-container').mCustomScrollbar('update');
				}, this), 500);
				var previewContBB = this._partsPreviewCont.getBoundingClientRect();
				this._previewContTop = previewContBB.top;
				var bottomBound = previewContBB.bottom + previewContBB.height / 2;

				$('#slide-sorter').mCustomScrollbar({
					axis: 'y',
					theme: 'dark-thick',
					scrollInertia: 0,
					alwaysShowScrollbar: 1,
					callbacks:{
						whileScrolling: function() {
							control._onScroll(this);
						}
					}
				});

				this._map.on('click', function() {
					this.partsFocused = false;
				}, this);

				this._map.on('keydown', function(e) {
					if (this.partsFocused === true) {
						switch (e.originalEvent.keyCode) {
						case 38:
							this._setPart('prev');
							break;
						case 40:
							this._setPart('next');
							break;
						}
					}
				}, this);

				this._scrollContainer = $('#slide-sorter .mCSB_container').get(0);

				// Add a special frame just as a drop-site for reordering.
				var frame = L.DomUtil.create('div', 'preview-frame', this._scrollContainer);
				this._addDnDHandlers(frame);
				frame.setAttribute('draggable', false);
				L.DomUtil.setStyle(frame, 'height', '20px');
				L.DomUtil.setStyle(frame, 'margin', '0em');

				// Create the preview parts
				for (var i = 0; i < parts; i++) {
					this._previewTiles.push(this._createPreview(i, e.partNames[i], bottomBound));
				}
				L.DomUtil.addClass(this._previewTiles[selectedPart], 'preview-img-currentpart');

				//-------------------open/close slide sorter with swipes------------
				//767px because at widths smaller than this the sidebar behaves like a mobile sidebar
				if (this.isMobileMode) {
					this.slideAnimationDuration = 250;//slide sorter open animation duration
					var presentationControlWrapper = $('#presentation-controls-wrapper');
					var documentContainerHammer = new Hammer(document.getElementById('document-container'));
					var presentationControlWrapperHammer = new Hammer(document.getElementById('presentation-controls-wrapper'));
					var self = this;
					//open slide sorter on right swipe on document view
					documentContainerHammer.on('swiperight', function (e) {
						var startX = e.changedPointers[0].clientX - e.deltaX;
						if (startX > window.screen.width * 0.2)//detect open swipes only near screen edge
							return;
						self._openMobileSidebar();
					});

					//close slide sorter on left swipe on it or the document view
					var closeSwipeElements = [presentationControlWrapperHammer, documentContainerHammer];
					closeSwipeElements.forEach(function (hammerElement) {
						hammerElement.on('swipeleft', function () {
							self._closeMobileSidebar();
						})
					});

					//show the slide sorter by default when the document is opened
					//show it only the first time, or if the user left it opened the last time
					if (window.localStorage.getItem('slideSelectorActive') == 1 || window.localStorage.getItem('slideSelectorActive') == undefined) {
						$(document).ready(function () {
							presentationControlWrapper.css({
								'z-index': 1001,
								'display': 'block',
								'top': '40px',
								'bottom': '0px',
								'max-width': screen.width + 'px',
								'width': screen.width + 'px'
							});
						})
						window.localStorage.setItem('slideSelectorActive', 1);
					}

				}
				//-------------------------------------------------------------

				this._previewInitialized = true;
			}
			else
			{
				if (e.partNames !== undefined) {
					this._syncPreviews(e);
				}

				// change the border style of the selected preview.
				for (var j = 0; j < parts; j++) {
					L.DomUtil.removeClass(this._previewTiles[j], 'preview-img-currentpart');
					L.DomUtil.removeClass(this._previewTiles[j], 'preview-img-selectedpart');
					if (j === selectedPart)
						L.DomUtil.addClass(this._previewTiles[j], 'preview-img-currentpart');
					else if (selectedParts.indexOf(j) >= 0)
						L.DomUtil.addClass(this._previewTiles[j], 'preview-img-selectedpart');
				}
			}
		}
	},

	_openMobileSidebar: function () {
		var presentationControlWrapper = $('#presentation-controls-wrapper');
		presentationControlWrapper.css({//first of all make it visible and move outside the screen
			'z-index': 1001,//above bottom bar
			'display': 'block',
			'left': -screen.width + 'px',
			'top': '40px',
			'bottom': '0px',
			'max-width': screen.width + 'px',
			'width': screen.width + 'px'
		}).animate({//then play an animation so it slides on screen
			'left': '0px'
		}, this.slideAnimationDuration, function () {
			window.localStorage.setItem('slideSelectorActive', 1);
		});
	},

	_closeMobileSidebar: function () {
		var presentationControlWrapper = $('#presentation-controls-wrapper');
		presentationControlWrapper.animate({//slide out of the screen
			'left': -screen.width + 'px',
		}, this.slideAnimationDuration, function () {
			//when the animation is done reset css properties that where added when opening it
			presentationControlWrapper.css({
				'z-index': '',
				'display': '',
				'top': '',
				'bottom': '',
				'max-width': '',
				'width': '',
				'left': ''
			});
			window.localStorage.setItem('slideSelectorActive', 0);
		});
	},

	_createPreview: function (i, hashCode, bottomBound) {
		var frame = L.DomUtil.create('div', 'preview-frame', this._scrollContainer);
		this._addDnDHandlers(frame);
		L.DomUtil.create('span', 'preview-helper', frame);
		var self = this;

		var imgClassName = 'preview-img';
		var img = L.DomUtil.create('img', imgClassName, frame);
		img.hash = hashCode;
		img.src = L.Icon.Default.imagePath + '/preview_placeholder.png';
		img.fetched = false;
		L.DomEvent
			.on(img, 'click', L.DomEvent.stopPropagation)
			.on(img, 'click', L.DomEvent.stop)
			.on(img, 'click', this._setPart, this)
			.on(img, 'click', this._map.focus, this._map)
			.on(img, 'click', function () {
				this.partsFocused = true;
			}, this)
			.on(img, 'click', function () {
				if (self.isMobileMode)
					self._closeMobileSidebar();
			});

		var topBound = this._previewContTop;
		var previewFrameTop = 0;
		var previewFrameBottom = 0;
		if (i > 0) {
			if (!bottomBound) {
				var previewContBB = this._partsPreviewCont.getBoundingClientRect();
				bottomBound = this._previewContTop + previewContBB.height + previewContBB.height / 2;
			}
			previewFrameTop = this._previewContTop + this._previewFrameMargin + i * (this._previewFrameHeight + this._previewFrameMargin);
			previewFrameTop -= this._scrollY;
			previewFrameBottom = previewFrameTop + this._previewFrameHeight;
			//when the sidebar is full screen on mobile devices this breaks the slides
			if (!this.isMobileMode)
				L.DomUtil.setStyle(img, 'height', this._previewImgHeight + 'px');
		}

		var imgSize;
		if (i === 0 || (previewFrameTop >= topBound && previewFrameTop <= bottomBound)
			|| (previewFrameBottom >= topBound && previewFrameBottom <= bottomBound) || this.isMobileMode) {
			//... || this.isMobileMode in the above if is required to get previews for all slides on mobile, otherwise only the first tile will have a preview
			imgSize = this._map.getPreview(i, i, 180, 180, {autoUpdate: this.options.autoUpdate});
			img.fetched = true;
			L.DomUtil.setStyle(img, 'height', '');
		}

		if (i === 0) {
			var previewImgBorder = Math.round(parseFloat(L.DomUtil.getStyle(img, 'border-top-width')));
			var previewImgMinWidth = Math.round(parseFloat(L.DomUtil.getStyle(img, 'min-width')));
			var imgHeight = imgSize.height;
			if (imgSize.width < previewImgMinWidth)
				imgHeight = Math.round(imgHeight * previewImgMinWidth / imgSize.width);
			var previewFrameBB = frame.getBoundingClientRect();
			this._previewFrameMargin = previewFrameBB.top - this._previewContTop;
			this._previewImgHeight = imgHeight;
			this._previewFrameHeight = imgHeight + 2 * previewImgBorder;
		}

		return img;
	},

	_setPart: function (e) {
		//helper function to check if the view is in the scrollview visible area
		function isVisible(el) {
			var elemRect = el.getBoundingClientRect();
			var elemTop = elemRect.top;
			var elemBottom = elemRect.bottom;
			var isVisible = (elemTop >= 0) && (elemBottom <= window.innerHeight);
			return isVisible;
		}
		if (e === 'prev' || e === 'next') {
			this._map.setPart(e);
			var node = $('#slide-sorter .mCSB_container .preview-frame')[this._map.getCurrentPartNumber()];
			if (!isVisible(node)) {
				if (e === 'prev') {
					setTimeout(function () {
						$('#slide-sorter').mCustomScrollbar('scrollTo', node);
					}, 50);
				} else {
					var nodeHeight = $(node).height();
					var sliderHeight= $('#slide-sorter').height();
					var nodePos = $(node).position().top;
					setTimeout(function () {
						$('#slide-sorter').mCustomScrollbar('scrollTo', nodePos-(sliderHeight-nodeHeight-nodeHeight/2));
					}, 50);
				}
			}
			return;
		}
		var part = $('#slide-sorter .mCSB_container .preview-frame').index(e.target.parentNode);
		if (part !== null) {
			var partId = parseInt(part) - 1; // The first part is just a drop-site for reordering.

			if (e.ctrlKey) {
				this._map.selectPart(partId, 2, false); // Toggle selection on ctrl+click.
			} else if (e.altKey) {
				console.log('alt');
			} else if (e.shiftKey) {
				console.log('shift');
			} else {
				this._map.setPart(partId);
				this._map.selectPart(partId, 1, false); // And select.
			}
		}
	},

	_updatePart: function (e) {
		if (e.docType === 'presentation' && e.part >= 0) {
			this._map.getPreview(e.part, e.part, 180, 180, {autoUpdate: this.options.autoUpdate});
		}
	},

	_syncPreviews: function (e) {
		var it = 0;
		var parts = e.parts;
		if (parts !== this._previewTiles.length) {
			if (Math.abs(parts - this._previewTiles.length) === 1) {
				if (parts > this._previewTiles.length) {
					for (it = 0; it < parts; it++) {
						if (it === this._previewTiles.length) {
							this._insertPreview({selectedPart: it - 1, hashCode: e.partNames[it]});
							break;
						}
						if (this._previewTiles[it].hash !== e.partNames[it]) {
							this._insertPreview({selectedPart: it, hashCode: e.partNames[it]});
							break;
						}
					}
				}
				else {
					for (it = 0; it < this._previewTiles.length; it++) {
						if (it === e.partNames.length ||
						    this._previewTiles[it].hash !== e.partNames[it]) {
							this._deletePreview({selectedPart: it});
							break;
						}
					}
				}
			}
			else {
				// sync all, should never happen
				while (this._previewTiles.length < e.partNames.length) {
					this._insertPreview({selectedPart: this._previewTiles.length - 1,
							     hashCode: e.partNames[this._previewTiles.length]});
				}

				while (this._previewTiles.length > e.partNames.length) {
					this._deletePreview({selectedPart: this._previewTiles.length - 1});
				}

				for (it = 0; it < e.partNames.length; it++) {
					this._previewTiles[it].hash = e.partNames[it];
					this._previewTiles[it].src = L.Icon.Default.imagePath + '/preview_placeholder.png';
					this._previewTiles[it].fetched = false;
				}
				this._onScrollEnd();
			}
		}
		else {
			// update hash code when user click insert slide.
			for (it = 0; it < parts; it++) {
				if (this._previewTiles[it].hash !== e.partNames[it]) {
					this._previewTiles[it].hash = e.partNames[it];
					this._map.getPreview(it, it, 180, 180, {autoUpdate: this.options.autoUpdate});
				}
			}
		}
	},

	_updatePreview: function (e) {
		if (this._map.getDocType() === 'presentation' || this._map.getDocType() === 'drawing') {
			if (!this._previewInitialized)
				return;
			this._previewTiles[e.id].src = e.tile;
		}
	},

	_updatePreviewIds: function () {
		$('#slide-sorter').mCustomScrollbar('update');
	},

	_insertPreview: function (e) {
		if (this._map.getDocType() === 'presentation') {
			var newIndex = e.selectedPart + 1;
			var newPreview = this._createPreview(newIndex, (e.hashCode === undefined ? null : e.hashCode));

			// insert newPreview to newIndex position
			this._previewTiles.splice(newIndex, 0, newPreview);

			var selectedFrame = this._previewTiles[e.selectedPart].parentNode;
			var newFrame = newPreview.parentNode;

			// insert after selectedFrame
			selectedFrame.parentNode.insertBefore(newFrame, selectedFrame.nextSibling);
			this._updatePreviewIds();
		}
	},

	_deletePreview: function (e) {
		if (this._map.getDocType() === 'presentation') {
			var selectedFrame = this._previewTiles[e.selectedPart].parentNode;
			L.DomUtil.remove(selectedFrame);

			this._previewTiles.splice(e.selectedPart, 1);
			this._updatePreviewIds();
		}
	},

	_onScroll: function (e) {
		var scrollOffset = 0;
		if (e) {
			var prevScrollY = this._scrollY;
			this._scrollY = -e.mcs.top;
			scrollOffset = this._scrollY - prevScrollY;
		}

		var previewContBB = this._partsPreviewCont.getBoundingClientRect();
		var extra =  previewContBB.height;
		var topBound = this._previewContTop - (scrollOffset < 0 ? extra : previewContBB.height / 2);
		var bottomBound = this._previewContTop + previewContBB.height + (scrollOffset > 0 ? extra : previewContBB.height / 2);
		for (var i = 0; i < this._previewTiles.length; ++i) {
			var img = this._previewTiles[i];
			if (img && img.parentNode && !img.fetched) {
				var previewFrameBB = img.parentNode.getBoundingClientRect();
				if ((previewFrameBB.top >= topBound && previewFrameBB.top <= bottomBound)
				|| (previewFrameBB.bottom >= topBound && previewFrameBB.bottom <= bottomBound)) {
					this._map.getPreview(i, i, 180, 180, {autoUpdate: this.options.autoUpdate});
					img.fetched = true;
				}
			}
		}
	},

	_addDnDHandlers: function (elem) {
		if (elem) {
			elem.setAttribute('draggable', true);
			elem.addEventListener('dragstart', this._handleDragStart, false);
			elem.addEventListener('dragenter', this._handleDragEnter, false)
			elem.addEventListener('dragover', this._handleDragOver, false);
			elem.addEventListener('dragleave', this._handleDragLeave, false);
			elem.addEventListener('drop', this._handleDrop, false);
			elem.addEventListener('dragend', this._handleDragEnd, false);
			elem.partsPreview = this;
		}
	},

	_handleDragStart: function (e) {
		// By default we move when dragging, but can
		// support duplication with ctrl in the future.
		e.dataTransfer.effectAllowed = 'move';
	},

	_handleDragOver: function (e) {
		if (e.preventDefault) {
			e.preventDefault();
		}

		// By default we move when dragging, but can
		// support duplication with ctrl in the future.
		e.dataTransfer.dropEffect = 'move';

		this.classList.add('preview-img-dropsite');
		return false;
	},

	_handleDragEnter: function () {
	},

	_handleDragLeave: function () {
		this.classList.remove('preview-img-dropsite');
	},

	_handleDrop: function (e) {
		if (e.stopPropagation) {
			e.stopPropagation();
		}

		var part = $('#slide-sorter .mCSB_container .preview-frame').index(e.target.parentNode);
		if (part !== null) {
			var partId = parseInt(part) - 1; // First frame is a drop-site for reordering.
			if (partId < 0)
				partId = -1; // First item is -1.
			this.partsPreview._map._socket.sendMessage('moveselectedclientparts position=' + partId);
			// Update previews, after a second, since we only get the dragged one invalidated.
			var that = this.partsPreview;
			setTimeout(function () {
				for (var i = 0; i < that._previewTiles.length; ++i) {
					that._map.getPreview(i, i, 180, 180, {autoUpdate: that.options.autoUpdate, broadcast: true});
				}
			}, 1000);
		}

		this.classList.remove('preview-img-dropsite');
		return false;
	},

	_handleDragEnd: function () {
		this.classList.remove('preview-img-dropsite');
	}

});

L.control.partsPreview = function (options) {
	return new L.Control.PartsPreview(options);
};
