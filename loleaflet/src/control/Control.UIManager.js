/* -*- js-indent-level: 8 -*- */
/*
 * L.Control.UIManager
 */

/* global $ setupToolbar w2ui w2utils */
L.Control.UIManager = L.Control.extend({
	onAdd: function (map) {
		this.map = map;
	},

	initializeBasicUI: function() {
		var menubar = L.control.menubar();
		this.map.menubar = menubar;
		this.map.addControl(menubar);

		this.map.addControl(L.control.statusBar());

		if (window.mode.isMobile()) {
			$('#toolbar-search').hide();
			$('#mobile-edit-button').show();
		} else {
			this.map.addControl(L.control.topToolbar());
			this.map.addControl(L.control.signingBar());
		}

		setupToolbar(this.map);

		this.map.addControl(L.control.scroll());
		this.map.addControl(L.control.alertDialog());
		this.map.addControl(L.control.mobileWizard());
		this.map.addControl(L.control.languageDialog());
		this.map.dialog = L.control.lokDialog();
		this.map.addControl(this.map.dialog);
		this.map.addControl(L.control.contextMenu());
		this.map.addControl(L.control.infobar());
		this.map.addControl(L.control.userList());
	},

	initializeSpecializedUI: function(docType) {
		var isDesktop = window.mode.isDesktop();

		if (window.mode.isMobile()) {
			this.map.addControl(L.control.mobileBottomBar(docType));
			this.map.addControl(L.control.mobileTopBar(docType));
			this.map.addControl(L.control.searchBar());
		}

		if (docType === 'spreadsheet') {
			this.map.addControl(L.control.sheetsBar({shownavigation: isDesktop}));
			this.map.addControl(L.control.formulaBar({showfunctionwizard: isDesktop}));
		}

		if (isDesktop && docType === 'presentation') {
			this.map.addControl(L.control.presentationBar());
		}

		if (window.mode.isMobile() || window.mode.isTablet()) {

			this.map.on('updatetoolbarcommandvalues', function() {
				w2ui['editbar'].refresh();
			});

			this.map.on('showbusy', function(e) {
				w2utils.lock(w2ui['actionbar'].box, e.label, true);
			});

			this.map.on('hidebusy', function() {
				// If locked, unlock
				if (w2ui['actionbar'].box.firstChild.className === 'w2ui-lock') {
					w2utils.unlock(w2ui['actionbar'].box);
				}
			});

			this.map.on('updatepermission', window.onUpdatePermission);
		}
	}
});

L.control.uiManager = function () {
	return new L.Control.UIManager();
};
