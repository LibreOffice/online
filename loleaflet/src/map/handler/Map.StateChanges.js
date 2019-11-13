/* -*- js-indent-level: 8 -*- */
/*
 * L.Map.StateChanges stores the state changes commands coming from core
 * LOK_CALLBACK_STATE_CHANGED callback
 */
 /* global $ */
 /*eslint no-extend-native:0*/
L.Map.mergeOptions({
	stateChangeHandler: true
});

L.Map.StateChangeHandler = L.Handler.extend({

	initialize: function (map) {
		this._map = map;
		// Contains the items for which state will be tracked
		// Stores the last received value from core ('true', 'false', 'enabled', 'disabled')
		this._items = {};
	},

	addHooks: function () {
		this._map.on('commandstatechanged', this._onStateChanged, this);
	},

	removeHooks: function () {
		this._map.off('commandstatechanged', this._onStateChanged, this);
	},

	_onStateChanged: function(e) {
		var index = e.state.indexOf('{');
		var state = index !== -1 ? JSON.parse(e.state.substring(index)) : e.state;
		this._items[e.commandName] = state;
		if (e.commandName === '.uno:CurrentTrackedChangeId') {
			var redlineId = 'change-' + state;
			this._map._docLayer._annotations.selectById(redlineId);
		}
		$('#document-container').removeClass('slide-master-mode');
		$('#document-container').addClass('slide-normal-mode');
		if (this._map['stateChangeHandler'].getItemValue('.uno:SlideMasterPage')) {
			$('#document-container').removeClass('slide-normal-mode');
			$('#document-container').addClass('slide-master-mode');
		}
		if (!this._map['stateChangeHandler'].getItemValue('.uno:SlideMasterPage') || this._map['stateChangeHandler'].getItemValue('.uno:SlideMasterPage') == 'false' || this._map['stateChangeHandler'].getItemValue('.uno:SlideMasterPage') == 'undefined') {
			$('#document-container').removeClass('slide-master-mode');
			$('#document-container').addClass('slide-normal-mode');
		}
	},

	getItems: function() {
		return this._items;
	},

	getItemValue: function(unoCmd) {
		if (unoCmd && unoCmd.substring(0, 5) !== '.uno:') {
			unoCmd = '.uno:' + unoCmd;
		}

		return this._items[unoCmd];
	}
});

L.Map.addInitHook('addHandler', 'stateChangeHandler', L.Map.StateChangeHandler);
