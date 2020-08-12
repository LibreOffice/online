/* -*- js-indent-level: 8 -*- */
/*
 * L.SVGGroup
 */

L.SVGGroup = L.Layer.extend({

	options: {
		noClip: true,
		manualDrag: false
	},

	initialize: function (bounds, options) {
		L.setOptions(this, options);
		this._pathNodeCollection = new L.Path.PathNodeCollection();
		this._bounds = bounds;
		this._rect = L.rectangle(bounds, this.options);
		this._hasSVGNode = false;
		if (L.Browser.touch && !L.Browser.pointer) {
			this.options.manualDrag = true;
		}

		this.on('dragstart scalestart rotatestart', this._showEmbeddedSVG, this);
		this.on('dragend scaleend rotateend', this._hideEmbeddedSVG, this);
	},

	setVisible: function (visible) {
		this._forEachSVGNode(function (svgNode) {
			if (visible)
				svgNode.setAttribute('visibility', 'visible');
			else
				svgNode.setAttribute('visibility', 'hidden');
		});
	},

	sizeSVG: function () {

		if (!this._hasSVGNode) {
			return;
		}

		var size = L.bounds(this._map.latLngToLayerPoint(this._bounds.getNorthWest()),
			this._map.latLngToLayerPoint(this._bounds.getSouthEast())).getSize();

		this._forEachSVGNode(function (svgNode) {
			svgNode.setAttribute('width', size.x);
			svgNode.setAttribute('height', size.y);
		});
	},

	parseSVG: function (svgString) {
		var parser = new DOMParser();
		return parser.parseFromString(svgString, 'image/svg+xml');
	},

	addEmbeddedSVG: function (svgString) {
		var svgDoc = this.parseSVG(svgString);

		if (svgDoc.lastChild.localName !== 'svg')
			return;

		var svgLastChild = svgDoc.lastChild;
		var thisObj = this;
		var nodeIndex = 0;
		this._forEachGroupNode(function (groupNode, rectNode, nodeData) {
			var lastChildClone = thisObj._cloneNodeWithIdSuffix(svgLastChild, '_' + nodeIndex);
			var svgNode = groupNode.insertBefore(lastChildClone, rectNode);
			nodeData.setCustomField('svg', svgNode);
			nodeData.setCustomField('dragShape', rectNode);
			thisObj._dragShapePresent = true;
			svgNode.setAttribute('pointer-events', 'none');
			svgNode.setAttribute('opacity', thisObj._dragStarted ? 1 : 0);
			nodeIndex += 1;
		});

		this._hasSVGNode = true;

		this.sizeSVG();
		this._update();
	},

	_onDragStart: function(evt) {
		if (!this._map || !this._dragShapePresent || !this.dragging)
			return;
		this._dragStarted = true;
		this._moved = false;

		if (!this.options.manualDrag) {
			this._forEachDragShape(function (dragShape) {
				L.DomEvent.on(dragShape, 'mousemove', this._onDrag, this);
				L.DomEvent.on(dragShape, 'mouseup', this._onDragEnd, this);
				if (this.dragging.constraint)
					L.DomEvent.on(dragShape, 'mouseout', this._onDragEnd, this);
			}.bind(this));
		}

		var data = {
			originalEvent: evt,
			containerPoint: this._map.mouseEventToContainerPoint(evt)
		};
		this.dragging._onDragStart(data);

		var pos = this._map.mouseEventToLatLng(evt);
		this.fire('graphicmovestart', {pos: pos});
	},

	_onDrag: function(evt) {
		if (!this._map || !this._dragShapePresent || !this.dragging)
			return;

		if (!this._moved) {
			this._moved = true;
			this._showEmbeddedSVG();
		}

		this.dragging._onDrag(evt);
	},

	_onDragEnd: function(evt) {
		if (!this._map || !this._dragShapePresent || !this.dragging)
			return;

		if (!this.options.manualDrag) {
			this._forEachDragShape(function (dragShape) {
				L.DomEvent.off(dragShape, 'mousemove', this._onDrag, this);
				L.DomEvent.off(dragShape, 'mouseup', this._onDragEnd, this);
				if (this.dragging.constraint)
					L.DomEvent.off(dragShape, 'mouseout', this._onDragEnd, this);
			}.bind(this));
		}

		this._moved = false;
		this._hideEmbeddedSVG();

		if (this._map) {
			var pos = this._map.mouseEventToLatLng(evt);
			this.fire('graphicmoveend', {pos: pos});
		}

		if (this.options.manualDrag || evt.type === 'mouseup')
			this.dragging._onDragEnd(evt);
		this._dragStarted = false;
	},

	bringToFront: function () {
		if (this._renderer) {
			this._renderer._bringToFront(this);
		}
		return this;
	},

	bringToBack: function () {
		if (this._renderer) {
			this._renderer._bringToBack(this);
		}
		return this;
	},

	getBounds: function () {
		return this._bounds;
	},

	getEvents: function () {
		return {};
	},

	onAdd: function () {
		this._dragStarted = false;
		this._renderer = this._map.getRenderer(this);
		this._renderer._initGroup(this);
		this._renderer._initPath(this._rect);
		this._renderer._addGroup(this);

		this._rect._map = this._map;
		this._rect._renderer = this._renderer;

		var svgDoc = this.options.svg ? this.parseSVG(this.options.svg) : undefined;

		var nodeIndex = -1;

		this._forEachGroupNode(function (groupNode, rectNode, nodeData) {

			nodeIndex += 1;

			if (!groupNode || !rectNode) {
				return;
			}

			L.DomUtil.addClass(groupNode, 'leaflet-control-buttons-disabled');

			if (svgDoc && svgDoc.lastChild.localName === 'svg') {
				var lastChildClone = this._cloneNodeWithIdSuffix(svgDoc.lastChild, '_' + nodeIndex);
				this._hasSVGNode = true;
				var svgNode = groupNode.appendChild(lastChildClone);
				nodeData.setCustomField('svg', svgNode);
				svgNode.setAttribute('opacity', 0);
				svgNode.setAttribute('pointer-events', 'none');
			}

			groupNode.appendChild(rectNode);
			nodeData.setCustomField('dragShape', rectNode);
			this._dragShapePresent = true;

			if (!this.options.manualDrag) {
				L.DomEvent.on(rectNode, 'mousedown', this._onDragStart, this);
			}
		}.bind(this));

		if (this.options.svg) {
			delete this.options.svg;
		}

		this.sizeSVG();

		this._update();
	},

	onRemove: function () {
		this._rect._map = this._rect._renderer = null;
		this._pathNodeCollection.forEachNode(function (nodeData) {

			var actualRenderer = nodeData.getActualRenderer();
			var rectNode = this._rect.getPathNode(actualRenderer);

			this.removeInteractiveTarget(rectNode);
			L.DomUtil.remove(rectNode);

		}.bind(this));

		this.removeEmbeddedSVG();
		this._renderer._removeGroup(this);
	},

	removeEmbeddedSVG: function () {
		if (!this._hasSVGNode) {
			return;
		}

		this._pathNodeCollection.forEachNode(function (nodeData) {
			var svgNode = nodeData.getCustomField('svg');
			L.DomUtil.remove(svgNode);
			nodeData.clearCustomField('svg');
		});

		this._dragShapePresent = false;
		this._hasSVGNode = false;
		this._update();
	},

	_hideEmbeddedSVG: function () {
		this._forEachSVGNode(function (svgNode) {
			svgNode.setAttribute('opacity', 0);
		});
	},

	_transform: function(matrix) {
		if (this._renderer) {
			if (matrix) {
				this._renderer.transformPath(this, matrix);
			} else {
				this._renderer._resetTransformPath(this);
				this._update();
			}
		}
		return this;
	},

	_project: function () {
		// console.log()
	},

	_reset: function () {
		this._update();
	},

	_showEmbeddedSVG: function () {
		this._forEachSVGNode(function (svgNode) {
			svgNode.setAttribute('opacity', 1);
		});
	},

	_update: function () {
		this._rect.setBounds(this._bounds);
		var point = this._map.latLngToLayerPoint(this._bounds.getNorthWest());

		this._forEachSVGNode(function (svgNode) {
			svgNode.setAttribute('x', point.x);
			svgNode.setAttribute('y', point.y);
		}.bind(this));
	},

	_updatePath: function () {
		this._update();
	},

	addPathNode: function (pathNode, actualRenderer) {

		this._path = undefined;

		if (!this._pathNodeCollection) {
			this._pathNodeCollection = new L.Path.PathNodeCollection();
		}

		this._pathNodeCollection.add(new L.Path.PathNodeData(pathNode, actualRenderer));
	},

	getPathNode: function (actualRenderer) {

		console.assert(this._pathNodeCollection, 'missing _pathNodeCollection member!');
		return this._pathNodeCollection.getPathNode(actualRenderer);
	},

	addClass: function (className) {
		this._pathNodeCollection.addOrRemoveClass(className, true /* add */);
	},

	removeClass: function (className) {
		this._pathNodeCollection.addOrRemoveClass(className, false /* add */);
	},

	_forEachGroupNode: function (callback) {

		var that = this;
		this._pathNodeCollection.forEachNode(function (nodeData) {

			var actualRenderer = nodeData.getActualRenderer();
			var groupNode = nodeData.getNode();
			var rectNode = that._rect.getPathNode(actualRenderer);

			callback(groupNode, rectNode, nodeData);

		});

		return true;
	},

	_forEachSVGNode: function (callback) {
		if (!this._hasSVGNode) {
			return false;
		}

		this._pathNodeCollection.forEachNode(function (nodeData) {
			var svgNode = nodeData.getCustomField('svg');
			if (svgNode) {
				callback(svgNode);
			}
		});

		return true;
	},

	_forEachDragShape: function (callback) {
		if (!this._dragShapePresent) {
			return false;
		}

		this._pathNodeCollection.forEachNode(function (nodeData) {
			var dragShape = nodeData.getCustomField('dragShape');
			if (dragShape) {
				callback(dragShape);
			}
		});

		return true;
	},

	_cloneNodeWithIdSuffix: function (srcNode, idSuffix) {
		console.assert((typeof idSuffix === 'string') && idSuffix, 'Invalid idSuffix parameter');
		console.assert(srcNode instanceof Element, 'invalid srcNode argument');

		var nodeClone = srcNode.cloneNode(true /* deep */);
		var nodeList = nodeClone.querySelectorAll('*');

		Array.prototype.forEach.call(nodeList, function (innerNode) {
			if (innerNode.id) {
				innerNode.id += idSuffix;
			}
		});

		return nodeClone;
	},

});

L.svgGroup = function (bounds, options) {
	return new L.SVGGroup(bounds, options);
};
