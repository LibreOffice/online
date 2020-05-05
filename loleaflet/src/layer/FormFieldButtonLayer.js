/* -*- js-indent-level: 8 -*- */
/*
 * L.FormFieldButton is used to interact with text based form fields.
 */
/* global $ */
L.FormFieldButton = L.Layer.extend({

	options: {
		pane: 'formfieldPane'
	},

	initialize: function (data) {
		console.assert(data.type === 'drop-down');
		this._buttonData = data;
	},

	onAdd: function (map) {
		this._clearButton();
		this._buildFormButton(map);
	},

	_buildFormButton: function(map) {
		// We use a container to have the frame and the drop-down button the same height
		var container = L.DomUtil.create('div', 'form-field-button-container', this.getPane('formfieldPane'));

		// Calculate button area in layer point unot
		var buttonArea = this._calculateButtonArea(map);

		// Build the frame around the text area
		var frameData = this._buildButtonFrame(container, buttonArea);
		var framePos = frameData[0];
		var frameWidth = frameData[1];
		var frameHeight = frameData[2];

		// We set the shared height here.
		container.style.height = frameHeight + 'px';

		// Add a drop down button to open the list
		this._buildDropDownButton(container, framePos, frameWidth);

		// Build list of items opened by clicking on the drop down button
		this._buildDropDownList(framePos, frameWidth, frameHeight);
	},

	_calculateButtonArea: function(map) {
		// First get the data from the message in twips.
		var strTwips = this._buttonData.textArea.match(/\d+/g);
		var topLeftTwips = new L.Point(parseInt(strTwips[0]), parseInt(strTwips[1]));
		var offset = new L.Point(parseInt(strTwips[2]), parseInt(strTwips[3]));
		var bottomRightTwips = topLeftTwips.add(offset);
		var buttonAreaTwips = [topLeftTwips, bottomRightTwips];

		// Then convert to unit which can be used on the layer.
		var buttonAreaLatLng = new L.LatLngBounds(
				map._docLayer._twipsToLatLng(buttonAreaTwips[0], this._map.getZoom()),
				map._docLayer._twipsToLatLng(buttonAreaTwips[1], this._map.getZoom()));

		var buttonAreaLayer = new L.Bounds(
				map.latLngToLayerPoint(buttonAreaLatLng.getNorthWest()),
				map.latLngToLayerPoint(buttonAreaLatLng.getSouthEast()));

		return buttonAreaLayer;
	},

	_buildButtonFrame: function(container, buttonArea) {
		// Create a frame around the text area
		var buttonFrame = L.DomUtil.create('div', 'form-field-frame', container);

		// Use a small padding between the text and the frame
		var extraPadding = 2;
		var size = buttonArea.getSize();
		var frameWidth = size.x + 1.5 * extraPadding;
		var frameHeight = size.y + 1.5 * extraPadding;
		buttonFrame.style.width = frameWidth + 'px';

		var framePos = new L.Point(buttonArea.min.x - extraPadding, buttonArea.min.y - extraPadding);
		L.DomUtil.setPosition(buttonFrame, framePos);

		return [framePos, frameWidth, frameHeight];
	},

	_buildDropDownButton: function(container, framePos, frameWidth) {
		var button = L.DomUtil.create('button', 'form-field-button', container);
		var buttonPos = new L.Point(framePos.x + frameWidth, framePos.y);
		L.DomUtil.setPosition(button, buttonPos);
		button.style.width = container.style.height;

		var image = L.DomUtil.create('img', 'form-field-button-image', button);
		image.src = 'images/unfold.svg';

		button.addEventListener('click', this._onClickDropDown);
	},

	_buildDropDownList: function(framePos, frameWidth, frameHeight) {
		var dropDownList = L.DomUtil.create('div', 'drop-down-field-list', this.getPane('formfieldPane'));
		$('.drop-down-field-list').hide();
		L.DomUtil.setPosition(dropDownList, framePos);
		dropDownList.style.minWidth = (frameWidth + frameHeight) + 'px';

		var itemList = this._buttonData.params.items;
		var selected = parseInt(this._buttonData.params.selected);
		for (var i = 0; i < itemList.length; ++i) {
			var option = L.DomUtil.create('div', 'drop-down-field-list-item', dropDownList);
			option.innerHTML = itemList[i];
			option.addEventListener('click', this._onListItemSelect);
			// Stop propagation to the main document
			option.addEventListener('mouseup', function(event) {event.stopPropagation();});
			option.addEventListener('mousedown', function(event) {event.stopPropagation();});

			if (i === selected)
				option.classList.add('selected');
		}
	},

	onRemove: function () {
		this._clearButton();
	},

	_onClickDropDown: function() {
		$('.drop-down-field-list').show();
	},

	_onListItemSelect: function(event) {
		$('.drop-down-field-list-item.selected').removeClass('selected');
		event.target.classList.add('selected');
		// TODO: send back
		$('.drop-down-field-list').hide();
		event.stopPropagation();
		console.warn(event.target.textContent);
	},

	_clearButton: function() {
		this.getPane('formfieldPane').innerHTML = '';
	}

});
