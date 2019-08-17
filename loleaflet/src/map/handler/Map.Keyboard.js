/* -*- js-indent-level: 8 -*- */
/*
 * L.Map.Keyboard is handling keyboard interaction with the map, enabled by default.
 */

L.Map.mergeOptions({
	keyboard: true,
	keyboardPanOffset: 20,
	keyboardZoomOffset: 1
});

L.Map.Keyboard = L.Handler.extend({

	keyModifier: {
		shift: 4096,
		ctrl: 8192,
		alt: 16384,
		ctrlMac: 32768
	},

	keymap: {
		8   : 1283, // backspace	: BACKSPACE
		9   : 1282, // tab 		: TAB
		13  : 1280, // enter 		: RETURN
		16  : null, // shift		: UNKOWN
		17  : null, // ctrl		: UNKOWN
		18  : null, // alt		: UNKOWN
		19  : null, // pause/break	: UNKOWN
		20  : null, // caps lock	: UNKOWN
		27  : 1281, // escape		: ESCAPE
		32  : 1284, // space		: SPACE
		33  : 1030, // page up		: PAGEUP
		34  : 1031, // page down	: PAGEDOWN
		35  : 1029, // end		: END
		36  : 1028, // home		: HOME
		37  : 1026, // left arrow	: LEFT
		38  : 1025, // up arrow		: UP
		39  : 1027, // right arrow	: RIGHT
		40  : 1024, // down arrow	: DOWN
		45  : 1285, // insert		: INSERT
		46  : 1286, // delete		: DELETE
		48  : 256,  // 0		: NUM0
		49  : 257,  // 1		: NUM1
		50  : 258,  // 2		: NUM2
		51  : 259,  // 3		: NUM3
		52  : 260,  // 4		: NUM4
		53  : 261,  // 5		: NUM5
		54  : 262,  // 6		: NUM6
		55  : 263,  // 7		: NUM7
		56  : 264,  // 8		: NUM8
		57  : 265,  // 9		: NUM9
		65  : 512,  // A		: A
		66  : 513,  // B		: B
		67  : 514,  // C		: C
		68  : 515,  // D		: D
		69  : 516,  // E		: E
		70  : 517,  // F		: F
		71  : 518,  // G		: G
		72  : 519,  // H		: H
		73  : 520,  // I		: I
		74  : 521,  // J		: J
		75  : 522,  // K		: K
		76  : 523,  // L		: L
		77  : 524,  // M		: M
		78  : 525,  // N		: N
		79  : 526,  // O		: O
		80  : 527,  // P		: P
		81  : 528,  // Q		: Q
		82  : 529,  // R		: R
		83  : 530,  // S		: S
		84  : 531,  // T		: T
		85  : 532,  // U		: U
		86  : 533,  // V		: V
		87  : 534,  // W		: W
		88  : 535,  // X		: X
		89  : 536,  // Y		: Y
		90  : 537,  // Z		: Z
		91  : null, // left window key	: UNKOWN
		92  : null, // right window key	: UNKOWN
		93  : null, // select key	: UNKOWN
		96  : 256,  // numpad 0		: NUM0
		97  : 257,  // numpad 1		: NUM1
		98  : 258,  // numpad 2		: NUM2
		99  : 259,  // numpad 3		: NUM3
		100 : 260,  // numpad 4		: NUM4
		101 : 261,  // numpad 5		: NUM5
		102 : 262,  // numpad 6		: NUM6
		103 : 263,  // numpad 7		: NUM7
		104 : 264,  // numpad 8		: NUM8
		105 : 265,  // numpad 9		: NUM9
		106 : 1289, // multiply		: MULTIPLY
		107 : 1287, // add		: ADD
		109 : 1288, // subtract		: SUBTRACT
		110 : 1309, // decimal point	: DECIMAL
		111 : 1290, // divide		: DIVIDE
		112 : 768,  // f1		: F1
		113 : 769,  // f2		: F2
		114 : 770,  // f3		: F3
		115 : 771,  // f4		: F4
		116 : 772,  // f5		: F5
		117 : 773,  // f6		: F6
		118 : 774,  // f7		: F7
		119 : 775,  // f8		: F8
		120 : 776,  // f9		: F9
		121 : 777,  // f10		: F10
		122 : 778,  // f11		: F11
		144 : 1313, // num lock		: NUMLOCK
		145 : 1314, // scroll lock	: SCROLLLOCK
		173 : 1288, // dash		: DASH (on Firefox)
		186 : 1317, // semi-colon	: SEMICOLON
		187 : 1295, // equal sign	: EQUAL
		188 : 1292, // comma		: COMMA
		189 : 1288, // dash		: DASH
		190 : null, // period		: UNKOWN
		191 : null, // forward slash	: UNKOWN
		192 : null, // grave accent	: UNKOWN
		219 : null, // open bracket	: UNKOWN
		220 : null, // back slash	: UNKOWN
		221 : null, // close bracket	: UNKOWN
		222 : null  // single quote	: UNKOWN
	},

	handleOnKeyDownKeys: {
		// these keys need to be handled on keydown in order for them
		// to work on chrome
		8   : true, // backspace
		9   : true, // tab
		19  : true, // pause/break
		20  : true, // caps lock
		27  : true, // escape
		33  : true, // page up
		34  : true, // page down
		35  : true, // end
		36  : true, // home
		37  : true, // left arrow
		38  : true, // up arrow
		39  : true, // right arrow
		40  : true, // down arrow
		45  : true, // insert
		46  : true, // delete
		113 : true  // f2
	},

	keyCodes: {
		pageUp:   33,
		pageDown: 34,
		enter:    13
	},

	navigationKeyCodes: {
		left:    [37],
		right:   [39],
		down:    [40],
		up:      [38],
		zoomIn:  [187, 107, 61, 171],
		zoomOut: [189, 109, 173]
	},

	initialize: function (map) {
		this._map = map;
		this._setPanOffset(map.options.keyboardPanOffset);
		this._setZoomOffset(map.options.keyboardZoomOffset);
		this.modifier = 0;
	},

	addHooks: function () {
		var container = this._map._container;

		// make the container focusable by tabbing
		if (container.tabIndex === -1) {
			container.tabIndex = '0';
		}

		this._map.on('mousedown', this._onMouseDown, this);
		this._map.on('keydown keyup keypress', this._onKeyDown, this);
		this._map.on('compositionstart compositionupdate compositionend textInput', this._onIME, this);
	},

	removeHooks: function () {
		this._map.off('mousedown', this._onMouseDown, this);
		this._map.off('keydown keyup keypress', this._onKeyDown, this);
		this._map.off('compositionstart compositionupdate compositionend textInput', this._onIME, this);
	},

	/*
	 * Returns true whenever the key event shall be ignored.
	 * This means shift+insert and shift+delete (or "insert or delete when holding
	 * shift down"). Those events are handled elsewhere to trigger "cut" and
	 * "paste" events, and need to be ignored in order to avoid double-handling them.
	 */
	_ignoreKeyEvent: function(e) {
		var shift = e.originalEvent.shiftKey;
		if ('key' in e.originalEvent) {
			var key = e.originalEvent.key;
			return (shift && (key === 'Delete' || key === 'Insert'));
		} else {
			// keyCode is not reliable in AZERTY/DVORAK keyboard layouts, is used
			// only as a fallback for MSIE8.
			var keyCode = e.originalEvent.keyCode;
			return (shift && (keyCode === 45 || keyCode === 46));
		}
	},

	_setPanOffset: function (pan) {
		var keys = this._panKeys = {},
		    codes = this.navigationKeyCodes,
		    i, len;

		for (i = 0, len = codes.left.length; i < len; i++) {
			keys[codes.left[i]] = [-1 * pan, 0];
		}
		for (i = 0, len = codes.right.length; i < len; i++) {
			keys[codes.right[i]] = [pan, 0];
		}
		for (i = 0, len = codes.down.length; i < len; i++) {
			keys[codes.down[i]] = [0, pan];
		}
		for (i = 0, len = codes.up.length; i < len; i++) {
			keys[codes.up[i]] = [0, -1 * pan];
		}
	},

	_setZoomOffset: function (zoom) {
		var keys = this._zoomKeys = {},
		    codes = this.navigationKeyCodes,
		    i, len;

		for (i = 0, len = codes.zoomIn.length; i < len; i++) {
			keys[codes.zoomIn[i]] = zoom;
		}
		for (i = 0, len = codes.zoomOut.length; i < len; i++) {
			keys[codes.zoomOut[i]] = -zoom;
		}
	},

	_onMouseDown: function () {
		this._map.notifyActive();
		if (this._map._permission === 'edit') {
			return;
		}
		this._map.focus();
	},

	// Convert javascript key codes to UNO key codes.
	_toUNOKeyCode: function (keyCode) {
		return this.keymap[keyCode] || keyCode;
	},

	_onKeyDown: function (e, keyEventFn, compEventFn, inputEle) {
		this._map.notifyActive();
		if (this._map.slideShow && this._map.slideShow.fullscreen) {
			return;
		}
		var docLayer = this._map._docLayer;
		if (!keyEventFn) {
			// default is to post keyboard events on the document
			keyEventFn = L.bind(docLayer._postKeyboardEvent, docLayer);
		}
		if (!compEventFn) {
			// document has winid=0
			compEventFn = L.bind(docLayer._postCompositionEvent, docLayer, 0 /* winid */);
		}
		if (!inputEle) {
			inputEle = this._map._clipboardContainer.activeElement();
		}
		this.modifier = 0;
		var shift = e.originalEvent.shiftKey ? this.keyModifier.shift : 0;
		var ctrl = e.originalEvent.ctrlKey ? this.keyModifier.ctrl : 0;
		var alt = e.originalEvent.altKey ? this.keyModifier.alt : 0;
		var cmd = e.originalEvent.metaKey ? this.keyModifier.ctrl : 0;
		var location = e.originalEvent.location;
		this._keyHandled = this._keyHandled || false;
		this.modifier = shift | ctrl | alt | cmd;

		// On Windows, pressing AltGr = Alt + Ctrl
		// Presence of AltGr is detected if previous Ctrl + Alt 'location' === 2 (i.e right)
		// because Ctrl + Alt + <some char> won't give any 'location' information.
		if (ctrl && alt) {
			if (e.type === 'keydown' && location === 2) {
				this._prevCtrlAltLocation = location;
				return;
			}
			else if (location === 1) {
				this._prevCtrlAltLocation = undefined;
			}

			if (this._prevCtrlAltLocation === 2 && location === 0) {
				// and we got the final character
				if (e.type === 'keypress') {
					ctrl = alt = this.modifier = 0;
				}
				else {
					// Don't handle remnant 'keyup'
					return;
				}
			}
		}

		if (ctrl || cmd) {
			if (this._handleCtrlCommand(e)) {
				return;
			}
		}

		var charCode = e.originalEvent.charCode;
		var keyCode = e.originalEvent.keyCode;

		if (!this._isComposing && e.type === 'keyup') {
			// not compositing and keyup, clear the input so it is ready
			// for next word (or char only)
			inputEle.value = '';
		}

		if ((this.modifier == this.keyModifier.alt || this.modifier == this.keyModifier.shift + this.keyModifier.alt) &&
		    keyCode >= 48) {
			// Presumably a Mac or iOS client accessing a "special character". Just ignore the alt modifier.
			// But don't ignore it for Alt + non-printing keys.
			this.modifier -= alt;
			alt = 0;
		}

		// handle help - F1
		if (e.type === 'keydown' && !shift && !ctrl && !alt && !cmd && keyCode === 112) {
			this._map.showHelp();
			e.originalEvent.preventDefault();
			return;
		}

		var unoKeyCode = this._toUNOKeyCode(keyCode);

		if (this.modifier) {
			unoKeyCode |= this.modifier;
			if (e.type !== 'keyup' && (this.modifier !== shift || (keyCode === 32 && !docLayer._isCursorVisible))) {
				keyEventFn('input', charCode, unoKeyCode);
				e.originalEvent.preventDefault();
				return;
			}
		}

		if (this._map._permission === 'edit') {
			docLayer._resetPreFetching();

			if (this._ignoreKeyEvent(e)) {
				// key ignored
			}
			else if (e.type === 'keydown') {
				this._keyHandled = false;

				if (this.handleOnKeyDownKeys[keyCode] && charCode === 0) {
					keyEventFn('input', charCode, unoKeyCode);
				}
			}
			else if ((e.type === 'keypress') && (!this.handleOnKeyDownKeys[keyCode] || charCode !== 0)) {
				if (charCode === keyCode && charCode !== 13) {
					// Chrome sets keyCode = charCode for printable keys
					// while LO requires it to be 0
					keyCode = 0;
					unoKeyCode = this._toUNOKeyCode(keyCode);
				}
				if (docLayer._debug) {
					// key press times will be paired with the invalidation messages
					docLayer._debugKeypressQueue.push(+new Date());
				}

				keyEventFn('input', charCode, unoKeyCode);
				this._keyHandled = true;
			}
			else if (e.type === 'keyup') {
				keyEventFn('up', charCode, unoKeyCode);
				this._keyHandled = true;
			}
			if (keyCode === 9) {
				// tab would change focus to other DOM elements
				e.originalEvent.preventDefault();
			}
		}
		else if (!this.modifier && (e.originalEvent.keyCode === 33 || e.originalEvent.keyCode === 34)) {
			// let the scrollbar handle page up / page down when viewing
			return;
		}
		else if (e.type === 'keydown') {
			var key = e.originalEvent.keyCode;
			var map = this._map;
			if (key in this._panKeys && !e.originalEvent.shiftKey) {
				if (map._panAnim && map._panAnim._inProgress) {
					return;
				}
				map.fire('scrollby', {x: this._panKeys[key][0], y: this._panKeys[key][1]});
			}
			else if (key in this._panKeys && e.originalEvent.shiftKey &&
					docLayer._selections.getLayers().length !== 0) {
				// if there is a selection and the user wants to modify it
				keyEventFn('input', charCode, unoKeyCode);
			}
			else if (key in this._zoomKeys) {
				map.setZoom(map.getZoom() + (e.shiftKey ? 3 : 1) * this._zoomKeys[key]);
			}
		}

		L.DomEvent.stopPropagation(e.originalEvent);
	},

	_onIME: function (e) {
		this._map.notifyActive();
		if (e.type === 'compositionstart') {
			this._isComposing = true; // we are starting composing with IME
		} else if (e.type === 'compositionupdate') {
			this._map._docLayer._postCompositionEvent(0, 'input', e.originalEvent.data);
		} else if (e.type === 'compositionend') {
			this._isComposing = false; // stop of composing with IME
			// get the composited char codes
			// clear the input now - best to do this ASAP so the input
			// is clear for the next word
			this._map._clipboardContainer.setValue('');
			// Set all keycodes to zero
			this._map._docLayer._postCompositionEvent(0, 'end', e.originalEvent.data);
		}

		if (e.type === 'textInput' && !this._keyHandled && !this._isComposing) {
			// Hack for making space and spell-check text insert work
			// in Chrome (on Andorid) or Chrome with IME.
			//
			// Chrome (Android) IME triggers keyup/keydown input with
			// code 229 when hitting space (as with all composiiton events)
			// with addition to 'textinput' event, in which we only see that
			// space was entered. Similar situation is also when inserting
			// a soft-keyboard spell-check item - it is visible only with
			// 'textinput' event (no composition event is fired).
			// To make this work we need to insert textinput.data here..
			//
			// TODO: Maybe make sure this is only triggered when keydown has
			// 229 code. Also we need to detect that composition was overriden
			// (part or whole word deleted) with the spell-checked word. (for
			// example: enter 'tar' and with spell-check correct that to 'rat')
			var data = e.originalEvent.data;
			for (var idx = 0; idx < data.length; idx++) {
				this._map._docLayer._postKeyboardEvent('input', data[idx].charCodeAt(), 0);
			}
		}
	},

	_handleCtrlCommand: function (e) {
		// Control
		if (e.originalEvent.keyCode == 17)
			return true;

		if (e.type !== 'keydown' && e.originalEvent.key !== 'c' && e.originalEvent.key !== 'v' && e.originalEvent.key !== 'x' &&
			/* Safari */ e.originalEvent.keyCode !== 99 && e.originalEvent.keyCode !== 118 && e.originalEvent.keyCode !== 120) {
			e.originalEvent.preventDefault();
			return true;
		}

		if (e.originalEvent.keyCode !== 67 && e.originalEvent.keyCode !== 86 && e.originalEvent.keyCode !== 88 &&
			/* Safari */ e.originalEvent.keyCode !== 99 && e.originalEvent.keyCode !== 118 && e.originalEvent.keyCode !== 120 &&
			e.originalEvent.key !== 'c' && e.originalEvent.key !== 'v' && e.originalEvent.key !== 'x') {
			// not copy or paste
			e.originalEvent.preventDefault();
		}

		if (e.originalEvent.ctrlKey && e.originalEvent.shiftKey && e.originalEvent.key === '?') {
			this._map.showLOKeyboardHelp();
			e.originalEvent.preventDefault();
			return true;
		}

		if (e.originalEvent.ctrlKey && (e.originalEvent.key === 'z' || e.originalEvent.key === 'Z')) {
			this._map._socket.sendMessage('uno .uno:Undo');
			e.originalEvent.preventDefault();
			return true;
		}

		if (e.originalEvent.ctrlKey && (e.originalEvent.key === 'y' || e.originalEvent.key === 'Y')) {
			this._map._socket.sendMessage('uno .uno:Redo');
			e.originalEvent.preventDefault();
			return true;
		}

		if (e.originalEvent.altKey || e.originalEvent.shiftKey) {

			// need to handle Ctrl + Alt + C separately for Firefox
			if (e.originalEvent.key === 'c' && e.originalEvent.altKey) {
				this._map.insertComment();
				return true;
			}

			// Ctrl + Alt
			if (!e.originalEvent.shiftKey) {
				switch (e.originalEvent.keyCode) {
				case 53: // 5
					this._map._socket.sendMessage('uno .uno:Strikeout');
					return true;
				case 70: // f
					this._map._socket.sendMessage('uno .uno:InsertFootnote');
					return true;
				case 67: // c
				case 77: // m
					this._map._socket.sendMessage('uno .uno:InsertAnnotation');
					return true;
				case 68: // d
					this._map._socket.sendMessage('uno .uno:InsertEndnote');
					return true;
				}
			} else if (e.originalEvent.altKey) {
				switch (e.originalEvent.keyCode) {
				case 68: // Ctrl + Shift + Alt + d for tile debugging mode
					this._map._docLayer.toggleTileDebugMode();
				}
			}

			return false;
		}

		switch (e.originalEvent.keyCode) {
		case 51: // 3
			if (this._map.getDocType() === 'spreadsheet') {
				this._map._socket.sendMessage('uno .uno:SetOptimalColumnWidthDirect');
				this._map._socket.sendMessage('commandvalues command=.uno:ViewRowColumnHeaders');
				return true;
			}
			return false;
		case 53: // 5
			if (this._map.getDocType() === 'spreadsheet') {
				this._map._socket.sendMessage('uno .uno:Strikeout');
				return true;
			}
			return false;
		case 67: // c
		case 88: // x
		case 99: // c (Safari)
		case 120: // x (Safari)
		case 91: // Left Cmd (Safari)
		case 93: // Right Cmd (Safari)
			// we prepare for a copy or cut event
			this._map._clipboardContainer.setValue(window.getSelection().toString());
			this._map.focus();
			this._map._clipboardContainer.select();
			return true;
		case 80: // p
			this._map.print();
			return true;
		case 83: // s
			this._map.fire('postMessage', {msgId: 'UI_Save'});
			if (!this._map._disableDefaultAction['UI_Save']) {
				this._map.save(false /* An explicit save should terminate cell edit */,
				               false /* An explicit save should save it again */);
			}
			return true;
		case 86: // v
		case 118: // v (Safari)
			return true;
		case 112: // f1
			this._map._socket.sendMessage('uno .uno:NoteVisible');
			return true;
		case 188: // ,
			this._map._socket.sendMessage('uno .uno:SubScript');
			return true;
		case 190: // .
			this._map._socket.sendMessage('uno .uno:SuperScript');
			return true;
		}
		if (e.type === 'keypress' && (e.originalEvent.ctrlKey || e.originalEvent.metaKey) &&
			(e.originalEvent.key === 'c' || e.originalEvent.key === 'v' || e.originalEvent.key === 'x')) {
			// need to handle this separately for Firefox
			return true;
		}
		return false;
	}
});

L.Map.addInitHook('addHandler', 'keyboard', L.Map.Keyboard);
