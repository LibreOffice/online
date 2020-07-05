/* -*- js-indent-level: 8 -*- */
/*
 * L.Bounds represents a rectangular area on the screen in pixel coordinates.
 */

L.Bounds = function (a, b) { //(Point, Point) or Point[]
	if (!a) { return; }

	var points = b ? [a, b] : a;

	for (var i = 0, len = points.length; i < len; i++) {
		this.extend(points[i]);
	}
};

L.Bounds.parse = function (rectString) { // (string) -> Bounds

	if (typeof rectString !== 'string') {
		console.error('invalid rectangle string');
		return undefined;
	}

	var rectParts = rectString.match(/\d+/g);
	if (rectParts.length < 4) {
		console.error('incomplete rectangle');
		return undefined;
	}

	var refPoint1 = new L.Point(parseInt(rectParts[0]), parseInt(rectParts[1]));
	var offset = new L.Point(parseInt(rectParts[2]), parseInt(rectParts[3]));
	var refPoint2 = refPoint1.add(offset);

	return new L.Bounds(refPoint1, refPoint2);
};

L.Bounds.prototype = {
	// extend the bounds to contain the given point
	extend: function (point) { // (Point)
		point = L.point(point);

		if (!this.min && !this.max) {
			this.min = point.clone();
			this.max = point.clone();
		} else {
			this.min.x = Math.min(point.x, this.min.x);
			this.max.x = Math.max(point.x, this.max.x);
			this.min.y = Math.min(point.y, this.min.y);
			this.max.y = Math.max(point.y, this.max.y);
		}
		return this;
	},

	clone: function () { // -> Bounds
		return new L.Bounds(this.min, this.max);
	},

	getCenter: function (round) { // (Boolean) -> Point
		return new L.Point(
		        (this.min.x + this.max.x) / 2,
		        (this.min.y + this.max.y) / 2, round);
	},

	getBottomLeft: function () { // -> Point
		return new L.Point(this.min.x, this.max.y);
	},

	getTopRight: function () { // -> Point
		return new L.Point(this.max.x, this.min.y);
	},

	getTopLeft: function () { // -> Point
		return new L.Point(this.min.x, this.min.y);
	},

	getBottomRight: function () { // -> Point
		return new L.Point(this.max.x, this.max.y);
	},

	getSize: function () {
		return this.max.subtract(this.min);
	},

	contains: function (obj) { // (Bounds) or (Point) -> Boolean
		var min, max;

		if (typeof obj[0] === 'number' || obj instanceof L.Point) {
			obj = L.point(obj);
		} else {
			obj = L.bounds(obj);
		}

		if (obj instanceof L.Bounds) {
			min = obj.min;
			max = obj.max;
		} else {
			min = max = obj;
		}

		return (min.x >= this.min.x) &&
		       (max.x <= this.max.x) &&
		       (min.y >= this.min.y) &&
		       (max.y <= this.max.y);
	},

	intersects: function (bounds) { // (Bounds) -> Boolean
		bounds = L.bounds(bounds);

		var min = this.min,
		    max = this.max,
		    min2 = bounds.min,
		    max2 = bounds.max,
		    xIntersects = (max2.x >= min.x) && (min2.x <= max.x),
		    yIntersects = (max2.y >= min.y) && (min2.y <= max.y);

		return xIntersects && yIntersects;
	},

	// non-destructive, returns a new Bounds
	add: function (point) { // (Point) -> Bounds
		return this.clone()._add(point);
	},

	// destructive, used directly for performance in situations where it's safe to modify existing Bounds
	_add: function (point) { // (Point) -> Bounds
		this.min._add(point);
		this.max._add(point);
		return this;
	},

	toString: function () {
		return '[' +
		        this.min.toString() + ', ' +
		        this.max.toString() + ']';
	},

	isValid: function () {
		return !!(this.min && this.max);
	}
};

L.bounds = function (a, b) { // (Bounds) or (Point, Point) or (Point[])
	if (!a || a instanceof L.Bounds) {
		return a;
	}
	return new L.Bounds(a, b);
};
