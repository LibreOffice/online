/*
 * L.WOPI contains WOPI related logic
 */

/* global $ w2ui toolbarUpMobileItems resizeToolbar _ */
L.Map.WOPI = L.Handler.extend({
	// If the CheckFileInfo call fails on server side, we won't have any PostMessageOrigin.
	// So use '*' because we still needs to send 'close' message to the parent frame which
	// wouldn't be possible otherwise.
	PostMessageOrigin: '*',
	BaseFileName: '',
	DocumentLoadedTime: false,
	HidePrintOption: false,
	HideSaveOption: false,
	HideExportOption: false,
	DisablePrint: false,
	DisableExport: false,
	DisableCopy: false,
	DisableInactiveMessages: false,
	UserCanNotWriteRelative: true,

	_appLoadedConditions: {
		docloaded: false,
		updatepermission: false,
		viewinfo: false /* Whether view information has already arrived */
	},

	_appLoaded: false,

	initialize: function(map) {
		this._map = map;
	},

	addHooks: function() {
		this._map.on('postMessage', this._postMessage, this);

		// init messages
		this._map.on('docloaded', this._postLoaded, this);
		this._map.on('updatepermission', this._postLoaded, this);
		// This indicates that 'viewinfo' message has already arrived
		this._map.on('viewinfo', this._postLoaded, this);

		this._map.on('wopiprops', this._setWopiProps, this);
		L.DomEvent.on(window, 'message', this._postMessageListener, this);
	},

	removeHooks: function() {
		this._map.off('postMessage', this._postMessage, this);

		// init messages
		this._map.off('docloaded', this._postLoaded, this);
		this._map.off('updatepermission', this._postLoaded, this);
		this._map.off('viewinfo', this._postLoaded, this);

		this._map.off('wopiprops', this._setWopiProps, this);
		L.DomEvent.off(window, 'message', this._postMessageListener, this);
	},

	_setWopiProps: function(wopiInfo) {
		// Store postmessageorigin property, if it exists
		if (wopiInfo['PostMessageOrigin']) {
			this.PostMessageOrigin = wopiInfo['PostMessageOrigin'];
		}

		this.BaseFileName = wopiInfo['BaseFileName'];
		this.HidePrintOption = !!wopiInfo['HidePrintOption'];
		this.HideSaveOption = !!wopiInfo['HideSaveOption'];
		this.HideExportOption = !!wopiInfo['HideExportOption'];
		this.DisablePrint = !!wopiInfo['DisablePrint'];
		this.DisableExport = !!wopiInfo['DisableExport'];
		this.DisableCopy = !!wopiInfo['DisableCopy'];
		this.DisableInactiveMessages = !!wopiInfo['DisableInactiveMessages'];
		this.UserCanNotWriteRelative = !!wopiInfo['UserCanNotWriteRelative'];

		this._map.fire('postMessage', {msgId: 'App_LoadingStatus', args: {Status: 'Frame_Ready'}});
	},

	resetAppLoaded: function() {
		this._appLoaded = false;
		for (var key in this._appLoadedConditions) {
			this._appLoadedConditions[key] = false;
		}
	},

	_postLoaded: function(e) {
		if (this._appLoaded) {
			return;
		}

		if (e.type === 'docloaded') {
			// doc unloaded
			if (!e.status)
			{
				this._appLoadedConditions[e.type] = false;
				return;
			}

			this.DocumentLoadedTime = Date.now();
		}
		this._appLoadedConditions[e.type] = true;
		for (var key in this._appLoadedConditions) {
			if (!this._appLoadedConditions[key])
				return;
		}

		this._appLoaded = true;
		this._map.fire('postMessage', {msgId: 'App_LoadingStatus', args: {Status: 'Document_Loaded', DocumentLoadedTime: this.DocumentLoadedTime}});
	},

	_postMessageListener: function(e) {
		if (!window.WOPIPostmessageReady) {
			return;
		}

		var msg = JSON.parse(e.data);
		if (msg.MessageId === 'Host_PostmessageReady') {
			// We already have a listener for this in loleaflet.html, so ignore it here
			return;
		}

		// allow closing documents before they are completely loaded
		if (msg.MessageId === 'Close_Session') {
			this._map._socket.sendMessage('closedocument');
			return;
		}

		// For all other messages, warn if trying to interact before we are completely loaded
		if (!this._appLoaded) {
			console.error('LibreOffice Online not loaded yet. Listen for App_LoadingStatus (Document_Loaded) event before using PostMessage API. Ignoring post message \'' + msg.MessageId + '\'.');
			return;
		}

		if (msg.MessageId === 'Insert_Button') {
			if (msg.Values) {
				if (msg.Values.id && !w2ui['toolbar-up'].get(msg.Values.id)
				    && msg.Values.imgurl) {
					if (this._map._permission === 'edit') {
						// add the css rule for the image
						$('html > head > style').append('.w2ui-icon.' + msg.Values.id + '{background: url(' + msg.Values.imgurl + ') no-repeat center !important; }');

						// add the item to the toolbar
						w2ui['toolbar-up'].insert('save', [
							{
								type: 'button',
								id: msg.Values.id,
								img: msg.Values.id,
								hint: _(msg.Values.hint), /* "Try" to localize ! */
								postmessage: true /* Notify the host back when button is clicked */
							}
						]);
						if (msg.Values.mobile)
						{
							// Add to our list of items to preserve when in mobile mode
							// FIXME: Wrap the toolbar in a class so that we don't make use
							// global variables and functions like this
							var idx = toolbarUpMobileItems.indexOf('save');
							toolbarUpMobileItems.splice(idx, 0, msg.Values.id);
						}
						resizeToolbar();
					}
					else if (this._map._permission === 'readonly') {
						// Just add a menu entry for it
						this._map.fire('addmenu', {id: msg.Values.id, label: msg.Values.hint});
					}
				}
			}
		}
		else if (msg.MessageId === 'Set_Settings') {
			if (msg.Values) {
				var alwaysActive = msg.Values.AlwaysActive;
				this._map.options.alwaysActive = !!alwaysActive;
			}
		}
		else if (msg.MessageId === 'Get_Views') {
			var getMembersRespVal = [];
			for (var viewInfoIdx in this._map._viewInfo) {
				getMembersRespVal.push({
					ViewId: viewInfoIdx,
					UserName: this._map._viewInfo[viewInfoIdx].username,
					UserId: this._map._viewInfo[viewInfoIdx].userid,
					UserExtraInfo: this._map._viewInfo[viewInfoIdx].userextrainfo,
					Color: this._map._viewInfo[viewInfoIdx].color
				});
			}

			this._postMessage({msgId: 'Get_Views_Resp', args: getMembersRespVal});
		}
		else if (msg.MessageId === 'Action_Save') {
			var dontTerminateEdit = msg.Values && msg.Values['DontTerminateEdit'];
			var dontSaveIfUnmodified = msg.Values && msg.Values['DontSaveIfUnmodified'];
			this._notifySave = msg.Values && msg.Values['Notify'];

			this._map.save(dontTerminateEdit, dontSaveIfUnmodified);
		}
		else if (msg.MessageId === 'Action_Print') {
			this._map.print();
		}
		else if (msg.MessageId === 'Action_Export') {
			if (msg.Values) {
				var format = msg.Values.Format;
				var fileName = this._map['wopi'].BaseFileName;
				fileName = fileName.substr(0, fileName.lastIndexOf('.'));
				fileName = fileName === '' ? 'document' : fileName;
				this._map.downloadAs(fileName + '.' + format, format);
			}
		}
		else if (msg.MessageId === 'Action_ShowBusy') {
			if (msg.Values && msg.Values.Label) {
				this._map.fire('showbusy', {label: msg.Values.Label});
			}
		}
		else if (msg.MessageId === 'Action_HideBusy') {
			this._map.fire('hidebusy');
		}
		else if (msg.MessageId === 'Get_Export_Formats') {
			var exportFormatsResp = [];
			for (var index in this._map._docLayer._exportFormats) {
				exportFormatsResp.push({
					Label: this._map._docLayer._exportFormats[index].label,
					Format: this._map._docLayer._exportFormats[index].format
				});
			}

			this._postMessage({msgId: 'Get_Export_Formats_Resp', args: exportFormatsResp});
		}
		else if (msg.MessageId === 'Action_SaveAs') {
			if (msg.Values) {
				if (msg.Values.Filename !== null && msg.Values.Filename !== undefined) {
					this._map.showBusy(_('Creating copy...'), false);
					this._map.saveAs(msg.Values.Filename);
				}
			}
		}
		else if (msg.MessageId === 'Host_VersionRestore') {
			if (msg.Values.Status === 'Pre_Restore') {
				this._map._socket.sendMessage('versionrestore prerestore');
			}
		}
	},

	_postMessage: function(e) {
		if (!this.enabled) { return; }
		var msgId = e.msgId;
		var values = e.args || {};
		if (!!this.PostMessageOrigin && window.parent !== window.self) {
			// Filter out unwanted save request response
			if (msgId === 'Action_Save_Resp') {
				if (!this._notifySave)
					return;

				this._notifySave = false;
			}

			var msg = {
				'MessageId': msgId,
				'SendTime': Date.now(),
				'Values': values
			};
			window.parent.postMessage(JSON.stringify(msg), this.PostMessageOrigin);
		}
	}
});

// This handler would only get 'enabled' by map if map.options.wopi = true
L.Map.addInitHook('addHandler', 'wopi', L.Map.WOPI);
