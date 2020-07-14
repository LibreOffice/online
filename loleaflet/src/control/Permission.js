/* -*- js-indent-level: 8 -*- */
/*
 * Document permission handler
 */
/* global $ _ vex */
L.Map.include({
	setPermission: function (perm) {
		if (perm === 'edit') {
			if (window.mode.isMobile() || window.mode.isTablet()) {
				var button = $('#mobile-edit-button');
				button.show();
				button.off('click');

				var that = this;
				button.on('click', function () {
					that._switchToEditMode();
				});

				// temporarily, before the user touches the floating action button
				this._enterReadOnlyMode('readonly');
			}
			else {
				this._enterEditMode(perm);
			}
		}
		else if (perm === 'view' || perm === 'readonly') {
			if (this.options.canTryUnlock) {
				// This status is not permanent. Allow to try to lock the file for edit again.
				button = $('#mobile-edit-button');
				// TODO: modify the icon here
				button.show();
				button.off('click');

				that = this;
				button.on('click', function () {
					that.on('attemptlockresult', _onAttemptLockResult);
					that._socket.sendMessage('attemptlock');
				});
			}
			else if (window.mode.isMobile() || window.mode.isTablet()) {
				$('#mobile-edit-button').hide();
			}

			this._enterReadOnlyMode(perm);
		}
	},

	_onAttemptLockResult: function (result) {
		this.off('attemptlockresult');
		if (result === 'ok') {
			this._switchToEditMode();
		}
		else {
			vex.dialog.alert({ message: _('The document could not be unlocked.') });
		}
	},

	_switchToEditMode: function () {
		$('#mobile-edit-button').hide();
		this._enterEditMode('edit');
		if (window.mode.isMobile() || window.mode.isTablet()) {
			this.fire('editorgotfocus');
			// In the iOS/android app, just clicking the mobile-edit-button is
			// not reason enough to pop up the on-screen keyboard.
			if (!(window.ThisIsTheiOSApp || window.ThisIsTheAndroidApp))
				this.focus();
		}
	},

	_enterEditMode: function (perm) {
		if (this.isPermissionReadOnly() && (window.mode.isMobile() || window.mode.isTablet())) {
			this.sendInitUNOCommands();
		}
		this._permission = perm;

		this._socket.sendMessage('requestloksession');
		if (!L.Browser.touch) {
			this.dragging.disable();
		}

		this.fire('updatepermission', {perm : perm});

		if (this._docLayer._docType === 'text') {
			this.setZoom(10);
		}
	},

	_enterReadOnlyMode: function (perm) {
		this._permission = perm;

		this.dragging.enable();
		// disable all user interaction, will need to add keyboard too
		if (this._docLayer) {
			this._docLayer._onUpdateCursor();
			this._docLayer._clearSelections();
			this._docLayer._onUpdateTextSelection();
		}
		this.fire('updatepermission', {perm : perm});
	},

	enableSelection: function () {
		if (this.isPermissionEdit()) {
			return;
		}
		this._socket.sendMessage('requestloksession');
		this.dragging.disable();
	},

	disableSelection: function () {
		if (this.isPermissionEdit()) {
			return;
		}
		this.dragging.enable();
	},

	isSelectionEnabled: function () {
		return !this.dragging.enabled();
	},

	getPermission: function () {
		return this._permission;
	},
	
	isPermissionEditForComments: function() {
		return true;
	},
	
	isPermissionReadOnly: function() {
		return this._permission === 'readonly';
	},
	
	isPermissionEdit: function() {
		return this._permission === 'edit';
	}
});
