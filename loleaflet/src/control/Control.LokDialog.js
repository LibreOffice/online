/*
 * L.Control.LokDialog used for displaying LOK dialogs
 */

/* global vex $ map */
L.Control.LokDialog = L.Control.extend({

	dialogIdPrefix: 'lokdialog-',

	onAdd: function (map) {
		map.on('window', this._onDialogMsg, this);
		map.on('windowpaint', this._onDialogPaint, this);
		map.on('opendialog', this._openDialog, this);
		map.on('docloaded', this._docLoaded, this);
	},

	_dialogs: {},

	_docLoaded: function(e) {
		if (!e.status) {
			$('.lokdialog_container').remove();
			$('.lokdialogchild-canvas').remove();
		}
	},

	_getParentDialog: function(id) {
		for (var winId in this._dialogs) {
			if (this._dialogs[winId].childid && this._dialogs[winId].childid === id) {
				return winId;
			}
		}
		return null;
	},

	_isOpen: function(dialogId) {
		return this._dialogs[dialogId] &&
			this._dialogs[dialogId].open &&
			$('#' + this._toDlgPrefix(dialogId)).length > 0;
	},

	_toRawDlgId: function(dialogId) {
		return dialogId.replace(this.dialogIdPrefix, '');
	},

	_toDlgPrefix: function(id) {
		return this.dialogIdPrefix + id;
	},

	// Create a rectangle string of form "x,y,width,height"
	// if params are missing, assumes 0,0,dialog width, dialog height
	_createRectStr: function(x, y, width, height) {
		if (!width)
			width = this._width;
		if (!height)
			height = this._height;
		if (!x)
			x = 0;
		if (!y)
			y = 0;

		return [x, y, width, height].join(',');
	},

	_sendPaintWindow: function(id, rectangle) {
		if (rectangle)
			rectangle = rectangle.replace(/ /g, '');

		this._map._socket.sendMessage('paintwindow ' + id + (rectangle ? ' rectangle=' + rectangle : ''));
	},

	_sendCloseWindow: function(id) {
		this._map._socket.sendMessage('windowcommand ' + id + ' close');
	},

	_isRectangleValid: function(rect) {
		rect = rect.split(',');
		if (parseInt(rect[0]) < 0 || parseInt(rect[1]) < 0 || parseInt(rect[2]) < 0 || parseInt(rect[3]) < 0)
			return false;
		return true;
	},

	_onDialogMsg: function(e) {
		var strDlgId = this._toDlgPrefix(e.id);
		if (e.action === 'created') {
			var width = parseInt(e.size.split(',')[0]);
			var height = parseInt(e.size.split(',')[1]);
			if (e.winType === 'dialog') {
				this._width = width;
				this._height = height;
				this._launchDialog(this._toDlgPrefix(e.id));
				this._sendPaintWindow(e.id, this._createRectStr());
			} else if (e.winType === 'child') {
				if (!this._isOpen(e.parentId))
					return;

				var parentId = parseInt(e.parentId);
				var left = parseInt(e.position.split(',')[0]);
				var top = parseInt(e.position.split(',')[1]);

				this._removeDialogChild(parentId);
				this._dialogs[parentId].childid = e.id;
				this._dialogs[parentId].childwidth = width;
				this._dialogs[parentId].childheight = height;
				this._dialogs[parentId].childx = left;
				this._dialogs[parentId].childy = top;
				this._createDialogChild(e.id, parentId, top, left);
				this._sendPaintWindow(e.id, this._createRectStr(0, 0, width, height));
			}
		} else if (e.action === 'invalidate') {
			var parent = this._getParentDialog(e.id);
			var rectangle = e.rectangle;
			if (parent) { // this is a floating window
				rectangle = '0,0,' + this._dialogs[parent].childwidth + ',' + this._dialogs[parent].childheight;
			} else { // this is the actual dialog
				if (rectangle && !this._isRectangleValid(rectangle))
					return;

				if (!rectangle)
					rectangle = '0,0,' + this._width + ',' + this._height;
			}
			this._sendPaintWindow(e.id, rectangle);
		} else if (e.action === 'size_changed') {
			this._width = parseInt(e.size.split(',')[0]);
			this._height = parseInt(e.size.split(',')[1]);

			strDlgId = this._toDlgPrefix(e.id);
			// FIXME: we don't really have to destroy and launch the dialog again but do it for
			// now because the size sent to us previously in 'created' cb is not correct
			$('#' + strDlgId).remove();
			this._launchDialog(strDlgId);
			$('#' + strDlgId).dialog('option', 'title', this._title);
			this._sendPaintWindow(e.id, this._createRectStr());
		} else if (e.action === 'cursor_invalidate') {
			if (this._isOpen(e.id) && !!e.rectangle) {
				var rectangle = e.rectangle.split(',');
				var x = parseInt(rectangle[0]);
				var y = parseInt(rectangle[1]);
				var height = parseInt(rectangle[3]);

				$('#' + strDlgId + '-cursor').css({height: height});
				// set the position of the lokdialog-cursor
				$(this._dialogs[e.id].cursor).css({left: x, top: y});
			}
		} else if (e.action === 'title_changed') {
			this._title = e.title;
			$('#' + strDlgId).dialog('option', 'title', e.title);
		} else if (e.action === 'cursor_visible') {
			var visible = e.visible === 'true';
			if (visible)
				$('#' + strDlgId + '-cursor').css({display: 'block'});
			else
				$('#' + strDlgId + '-cursor').css({display: 'none'});
		} else if (e.action === 'close') {
			parent = this._getParentDialog(e.id);
			if (parent)
				this._onDialogChildClose(this._toDlgPrefix(parent));
			else
				this._onDialogClose(e.id, false);
		}
	},

	_openDialog: function(e) {
		this._map.sendUnoCommand(e.uno);
	},

	_launchDialogCursor: function(dialogId) {
		var id = this._toRawDlgId(dialogId);
		this._dialogs[id].cursor = L.DomUtil.create('div', 'leaflet-cursor-container', L.DomUtil.get(dialogId));
		var cursor = L.DomUtil.create('div', 'leaflet-cursor lokdialog-cursor', this._dialogs[id].cursor);
		cursor.id = dialogId + '-cursor';
		L.DomUtil.addClass(cursor, 'blinking-cursor');
	},

	_launchDialog: function(strDlgId) {
		var canvas = '<div class="lokdialog" style="padding: 0px; margin: 0px; overflow: hidden;" id="' + strDlgId + '">' +
		    '<canvas class="lokdialog_canvas" tabindex="0" id="' + strDlgId + '-canvas" width="' + this._width + 'px" height="' + this._height + 'px"></canvas>' +
		    '</div>';
		$(document.body).append(canvas);
		var that = this;
		$('#' + strDlgId).dialog({
			width: this._width,
			title: 'LOK Dialog', // TODO: Get the 'real' dialog title from the backend
			modal: false,
			closeOnEscape: true,
			resizable: false,
			dialogClass: 'lokdialog_container',
			close: function() {
				that._onDialogClose(that._toRawDlgId(strDlgId), true);
			}
		});

		this._dialogs[this._toRawDlgId(strDlgId)] = { open: true };

		// don't make 'TAB' focus on this button; we want to cycle focus in the lok dialog with each TAB
		$('.lokdialog_container button.ui-dialog-titlebar-close').attr('tabindex', '-1').blur();

		$('#' + strDlgId + '-canvas').on('mousedown', function(e) {
			var buttons = 0;
			buttons |= e.button === map['mouse'].JSButtons.left ? map['mouse'].LOButtons.left : 0;
			buttons |= e.button === map['mouse'].JSButtons.middle ? map['mouse'].LOButtons.middle : 0;
			buttons |= e.button === map['mouse'].JSButtons.right ? map['mouse'].LOButtons.right : 0;
			var modifier = 0;
			that._postWindowMouseEvent('buttondown', strDlgId, e.offsetX, e.offsetY, 1, buttons, modifier);
		});
		$('#' + strDlgId + '-canvas').on('mouseup', function(e) {
			var buttons = 0;
			buttons |= e.button === map['mouse'].JSButtons.left ? map['mouse'].LOButtons.left : 0;
			buttons |= e.button === map['mouse'].JSButtons.middle ? map['mouse'].LOButtons.middle : 0;
			buttons |= e.button === map['mouse'].JSButtons.right ? map['mouse'].LOButtons.right : 0;
			var modifier = 0;
			that._postWindowMouseEvent('buttonup', strDlgId, e.offsetX, e.offsetY, 1, buttons, modifier);
		});
		$('#' + strDlgId + '-canvas').on('keyup keypress keydown', function(e) {
			e.strDlgId = strDlgId;
			that._handleDialogKeyEvent(e);
		});
		$('#' + strDlgId + '-canvas').on('contextmenu', function() {
			return false;
		});

		this._launchDialogCursor(strDlgId);
	},

	_postWindowMouseEvent: function(type, winid, x, y, count, buttons, modifier) {
		this._map._socket.sendMessage('windowmouse id=' + this._toRawDlgId(winid) +  ' type=' + type +
		                              ' x=' + x + ' y=' + y + ' count=' + count +
		                              ' buttons=' + buttons + ' modifier=' + modifier);
	},

	_postWindowKeyboardEvent: function(type, winid, charcode, keycode) {
		this._map._socket.sendMessage('windowkey id=' + this._toRawDlgId(winid) + ' type=' + type +
		                              ' char=' + charcode + ' key=' + keycode);
	},

	_handleDialogKeyEvent: function(e) {
		var docLayer = this._map._docLayer;
		this.modifier = 0;
		var shift = e.originalEvent.shiftKey ? this._map['keyboard'].keyModifier.shift : 0;
		var ctrl = e.originalEvent.ctrlKey ? this._map['keyboard'].keyModifier.ctrl : 0;
		var alt = e.originalEvent.altKey ? this._map['keyboard'].keyModifier.alt : 0;
		var cmd = e.originalEvent.metaKey ? this._map['keyboard'].keyModifier.ctrl : 0;
		var location = e.originalEvent.location;
		this.modifier = shift | ctrl | alt | cmd;

		var charCode = e.originalEvent.charCode;
		var keyCode = e.originalEvent.keyCode;
		var unoKeyCode = this._map['keyboard']._toUNOKeyCode(keyCode);

		if (this.modifier) {
			unoKeyCode |= this.modifier;
			if (e.type !== 'keyup') {
				this._postWindowKeyboardEvent('input', e.strDlgId, charCode, unoKeyCode);
				return;
			}
		}

		if (e.type === 'keydown' && this._map['keyboard'].handleOnKeyDownKeys[keyCode]) {
			this._postWindowKeyboardEvent('input', e.strDlgId, charCode, unoKeyCode);
		}
		else if (e.type === 'keypress' && (!this._map['keyboard'].handleOnKeyDownKeys[keyCode] || charCode !== 0)) {
			if (charCode === keyCode && charCode !== 13) {
				keyCode = 0;
				unoKeyCode = this._map['keyboard']._toUNOKeyCode(keyCode);
			}
			this._postWindowKeyboardEvent('input', e.strDlgId, charCode, unoKeyCode);
		}
		else if (e.type === 'keyup') {
			this._postWindowKeyboardEvent('up', e.strDlgId, charCode, unoKeyCode);
		}
	},

	_onDialogClose: function(dialogId, notifyBackend) {
		if (notifyBackend)
			this._sendCloseWindow(dialogId);
		$('#' + this._toDlgPrefix(dialogId)).remove();
		this._map.focus();
		delete this._dialogs[dialogId];
	},

	_paintDialog: function(dialogId, rectangle, imgData) {
		if (!this._isOpen(dialogId))
			return;

		var strDlgId = this._toDlgPrefix(dialogId);
		var img = new Image();
		var canvas = document.getElementById(strDlgId + '-canvas');
		var ctx = canvas.getContext('2d');
		img.onload = function() {
			var x = 0;
			var y = 0;
			if (rectangle) {
				rectangle = rectangle.split(',');
				x = parseInt(rectangle[0]);
				y = parseInt(rectangle[1]);
			}

			ctx.drawImage(img, x, y);
		};
		img.src = imgData;
	},

	// Binary dialog msg recvd from core
	_onDialogPaint: function (e) {
		var parent = this._getParentDialog(e.id);
		if (parent) {
			this._paintDialogChild(parent, e.width, e.height, e.rectangle, e.img);
		} else {
			this._paintDialog(e.id, e.rectangle, e.img);
		}
	},

	// Dialog Child Methods

	_paintDialogChild: function(dialogId, width, height, rectangle, imgData) {
		var strDlgId = this._toDlgPrefix(dialogId);
		var img = new Image();
		var canvas = document.getElementById(strDlgId + '-floating');
		canvas.width = width;
		canvas.height = height;
		var ctx = canvas.getContext('2d');
		img.onload = function() {
			ctx.drawImage(img, 0, 0);
		};
		img.src = imgData;

		// increase the height of the container,
		// so that if the floating window goes out of the parent,
		// it doesn't get stripped off
		height = parseInt(canvas.style.top) + canvas.height;
		var currentHeight = parseInt($('#' + strDlgId).css('height'));
		if (height > currentHeight)
			$('#' + strDlgId).css('height', height + 'px');
	},

	_onDialogChildClose: function(dialogId) {
		$('#' + dialogId + '-floating').remove();
		// remove any extra height allocated for the parent container
		var canvasHeight = document.getElementById(dialogId + '-canvas').height;
		$('#' + dialogId).height(canvasHeight + 'px');
	},

	_removeDialogChild: function(id) {
		$('#' + id + '-floating').remove();
	},

	_createDialogChild: function(childId, dialogId, top, left) {
		var strDlgId = this._toDlgPrefix(dialogId);
		var floatingCanvas = '<canvas class="lokdialogchild-canvas" id="' + strDlgId + '-floating"></canvas>';
		$('#' + strDlgId).append(floatingCanvas);
		$('#' + strDlgId + '-floating').css({position: 'absolute', left: left, top: top});

		var that = this;
		// attach events
		$('#' + strDlgId + '-floating').on('mousedown', function(e) {
			var buttons = 0;
			buttons |= e.button === map['mouse'].JSButtons.left ? map['mouse'].LOButtons.left : 0;
			buttons |= e.button === map['mouse'].JSButtons.middle ? map['mouse'].LOButtons.middle : 0;
			buttons |= e.button === map['mouse'].JSButtons.right ? map['mouse'].LOButtons.right : 0;
			var modifier = 0;
			that._postWindowMouseEvent('buttondown', childId, e.offsetX, e.offsetY, 1, buttons, modifier);
		});

		$('#' + strDlgId + '-floating').on('mouseup', function(e) {
			var buttons = 0;
			buttons |= e.button === map['mouse'].JSButtons.left ? map['mouse'].LOButtons.left : 0;
			buttons |= e.button === map['mouse'].JSButtons.middle ? map['mouse'].LOButtons.middle : 0;
			buttons |= e.button === map['mouse'].JSButtons.right ? map['mouse'].LOButtons.right : 0;
			var modifier = 0;
			that._postWindowMouseEvent('buttonup', childId, e.offsetX, e.offsetY, 1, buttons, modifier);
		});

		$('#' + strDlgId + '-floating').on('mousemove', function(e) {
			that._postWindowMouseEvent('move', childId, e.offsetX, e.offsetY, 1, 0, 0);
		});

		$('#' + strDlgId + '-floating').on('contextmenu', function() {
			return false;
		});
	}
});

L.control.lokDialog = function (options) {
	return new L.Control.LokDialog(options);
};
