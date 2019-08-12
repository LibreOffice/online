/* -*- js-indent-level: 8 -*- */
/*
 * Table Overlay
 */

L.TileLayer.include({
	_initializeTableOverlay: function () {
		this._tableColumnMarkers = [];
		this._tableRowMarkers = [];
		this._tableMarkersDragged = false;
	},
	_setMarkerPosition: function(marker) {
		var point = this._twipsToLatLng(marker._pointTwips, this._map.getZoom());
		point = this._map.project(point);
		var markerRect = marker._icon.getBoundingClientRect();
		if (marker._type.startsWith('column'))
			point = point.subtract(new L.Point(markerRect.width / 2, 3 * markerRect.height / 2));
		else
			point = point.subtract(new L.Point(3 * markerRect.width / 2, markerRect.height / 2));
		point = this._map.unproject(point);
		marker.setLatLng(point);
	},
	_createMarker: function(markerType, entry, baseline) {
		var className;
		if (markerType === 'column')
			className = 'table-column-resize-marker';
		else 
			className = 'table-row-resize-marker';

		var marker = L.marker(new L.LatLng(0, 0), {
			icon: L.divIcon({
				className: className,
				iconSize: null
			}),
			draggable: true
		});
		this._map.addLayer(marker);
		marker._type = markerType + '-' + entry.type;
		marker._position = parseInt(entry.position);
		marker._min = parseInt(entry.min);
		marker._max = parseInt(entry.max);
		if (markerType === 'column')
			marker._pointTwips = new L.Point(this._tablePositionOffset + marker._position, baseline);
		else
			marker._pointTwips = new L.Point(baseline, this._tablePositionOffset + marker._position);
			
		this._setMarkerPosition(marker);
		marker.on('dragstart drag dragend', this._onTableResizeMarkerDrag, this);
		return marker;
	},
	_onTableSelectedMsg: function (textMsg) {
		if (this._tableMarkersDragged == true) {
			return;
		}

		// Clean-up first
		var markerIndex;
		for (markerIndex = 0; markerIndex < this._tableColumnMarkers.length; markerIndex++) {
			this._map.removeLayer(this._tableColumnMarkers[markerIndex]);
		}
		this._tableColumnMarkers = [];

		for (markerIndex = 0; markerIndex < this._tableRowMarkers.length; markerIndex++) {
			this._map.removeLayer(this._tableRowMarkers[markerIndex]);
		}
		this._tableRowMarkers = [];

		// Parse the message
		textMsg = textMsg.substring('tableselected:'.length + 1);
		var message = JSON.parse(textMsg);		

		// Create markers
		if (message.rows && message.rows.entries.length > 0 && message.columns && message.columns.entries.length > 0) {
			this._tablePositionOffset = parseInt(message.rows.tableOffset);
			var firstRowPosition = parseInt(message.rows.left) + this._tablePositionOffset;
			var firstColumnPosition = parseInt(message.columns.left) + this._tablePositionOffset;
			var markerX, i, entry;

			entry = { type: 'left', 'position': message.columns.left };
			markerX = this._createMarker('column', entry, firstRowPosition);
			this._tableColumnMarkers.push(markerX);

			for (i = 0; i < message.columns.entries.length; i++) {
				entry = message.columns.entries[i];
				entry.type = 'middle';
				markerX = this._createMarker('column', entry, firstRowPosition);
				this._tableColumnMarkers.push(markerX);
			}

			entry = { type: 'right', position: message.columns.right };
			markerX = this._createMarker('column', entry, firstRowPosition);
			this._tableColumnMarkers.push(markerX);

			entry = { type: 'left', position: message.rows.left };
			markerX = this._createMarker('row', entry, firstColumnPosition);
			this._tableRowMarkers.push(markerX);

			for (i = 0; i < message.rows.entries.length; i++) {
				entry = message.rows.entries[i];
				entry.type = 'middle';
				markerX = this._createMarker('row', entry, firstColumnPosition);
				this._tableRowMarkers.push(markerX);
			}

			entry = { type: 'right', position: message.rows.right };
			markerX = this._createMarker('row', entry, firstColumnPosition);
			this._tableRowMarkers.push(markerX);
		}
	},

	// Update dragged text selection.
	_onTableResizeMarkerDrag: function (e) {
		if (e.type === 'dragstart') {
			e.target.isDragged = true;
			this._tableMarkersDragged = true;
		}
		else if (e.type === 'dragend') {
			e.target.isDragged = false;
			this._tableMarkersDragged = false;
		}

		if (e.type === 'dragend' || e.type === 'drag') {
			// modify the mouse position - move to center of the marker
			var aMousePosition = e.target.getLatLng();
			aMousePosition = this._map.project(aMousePosition);
			var size = e.target._icon.getBoundingClientRect();
			aMousePosition = aMousePosition.add(new L.Point(size.width / 2, size.height / 2));
			aMousePosition = this._map.unproject(aMousePosition);
			aMousePosition = this._latLngToTwips(aMousePosition);

			var newPosition;
			if (e.target._type.startsWith('column'))
				newPosition = aMousePosition.x - this._tablePositionOffset;
			else
				newPosition = aMousePosition.y - this._tablePositionOffset;

			if (e.target._type === 'column-middle' || e.target._type === 'row-middle') {
				if (newPosition < e.target._min)
					newPosition = e.target._min;
				else if (newPosition > e.target._max)
					newPosition = e.target._max;
			}

			// only if the position changes, otherwise don't do anything
			e.target._position = newPosition;

			var columns = {};
			var rows = {};
			var entries, i, marker;

			switch (e.target._type) {
			case 'column-left':
				columns.left = e.target._position;
				break;
			case 'column-right':
				columns.right = e.target._position;
				break;
			case 'column-middle':
				entries = [];
				for (i = 0; i < this._tableColumnMarkers.length; i++) {
					marker = this._tableColumnMarkers[i];
					if (marker._type === 'column-middle')
						entries.push({'position' : marker._position});
				}
				columns.entries = entries;
				break;
			case 'row-left':
				rows.left = e.target._position;
				break;
			case 'row-right':
				rows.right = e.target._position;
				break;
			case 'row-middle':
				entries = [];
				for (i = 0; i < this._tableRowMarkers.length; i++) {
					marker = this._tableRowMarkers[i];
					if (marker._type === 'row-middle')
						entries.push({'position' : marker._position});
				}
				rows = {'entries' : entries};
				break;
			}
			var resultObject = {
				'table': {
					'columns': columns,
					'rows': rows
				}
			};

			var blob = new Blob(['changecurrentobjectproperties\n', JSON.stringify(resultObject)]);
			this._map._socket.sendMessage(blob);
		}

		if (e.originalEvent)
			e.originalEvent.preventDefault();
	}
});

