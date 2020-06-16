/* -*- js-indent-level: 8 -*- */
/*
 * L.Control.UIManager - initializes the UI elements like toolbars, menubar or ruler
                         and allows to controll them (show/hide)
 */

/* global $ setupToolbar w2ui w2utils */
L.Control.UIManager = L.Control.extend({
	onAdd: function (map) {
		this.map = map;

		map.on('updatepermission', this.onUpdatePermission, this);
	},

	// UI initialization

	initializeBasicUI: function() {
		if (window.mode.isMobile() || !window.enableNotebookbar) {
			var menubar = L.control.menubar();
			this.map.menubar = menubar;
			this.map.addControl(menubar);
		}

		if (window.mode.isMobile()) {
			$('#mobile-edit-button').show();
		} else {
			if (!window.enableNotebookbar) {
				this.map.addControl(L.control.topToolbar());
			}

			this.map.addControl(L.control.signingBar());
			this.map.addControl(L.control.statusBar());
		}

		setupToolbar(this.map);

		this.map.addControl(L.control.documentNameInput());
		this.map.addControl(L.control.scroll());
		this.map.addControl(L.control.alertDialog());
		this.map.addControl(L.control.mobileWizard());
		this.map.addControl(L.control.languageDialog());
		this.map.dialog = L.control.lokDialog();
		this.map.addControl(this.map.dialog);
		this.map.addControl(L.control.contextMenu());
		this.map.addControl(L.control.infobar());
		this.map.addControl(L.control.userList());

		this.map.on('showbusy', function(e) {
			if (w2ui['actionbar'])
				w2utils.lock(w2ui['actionbar'].box, e.label, true);
		});

		this.map.on('hidebusy', function() {
			// If locked, unlock
			if (w2ui['actionbar'] && w2ui['actionbar'].box.firstChild.className === 'w2ui-lock') {
				w2utils.unlock(w2ui['actionbar'].box);
			}
		});
	},

	initializeSpecializedUI: function(docType) {
		var isDesktop = window.mode.isDesktop();

		if (window.mode.isMobile()) {
			this.map.addControl(L.control.mobileBottomBar(docType));
			this.map.addControl(L.control.mobileTopBar(docType));
			this.map.addControl(L.control.searchBar());
		} else if (window.enableNotebookbar) {
			if (docType === 'spreadsheet') {
				this.map.addControl(L.control.notebookbarCalc());
			} else if (docType === 'presentation') {
				this.map.addControl(L.control.notebookbarImpress());
			} else {
				this.map.addControl(L.control.notebookbarWriter());
			}

			var additionalOffset = 0;
			if (docType === 'spreadsheet') {
				additionalOffset = 56;
			}

			this.moveObjectVertically($('#spreadsheet-row-column-frame'), 121);
			this.moveObjectVertically($('#document-container'), 84 + additionalOffset);
			this.moveObjectVertically($('#presentation-controls-wrapper'), 84);
			this.moveObjectVertically($('#sidebar-dock-wrapper'), 43);
		}

		if (docType === 'spreadsheet') {
			this.map.addControl(L.control.sheetsBar({shownavigation: isDesktop || window.mode.isTablet()}));
			this.map.addControl(L.control.formulaBar());
		}

		if (docType === 'presentation' && (isDesktop || window.mode.isTablet())) {
			this.map.addControl(L.control.presentationBar());
		}

		if (window.mode.isMobile() || window.mode.isTablet()) {
			this.map.on('updatetoolbarcommandvalues', function() {
				w2ui['editbar'].refresh();
			});
		}
	},

	// Menubar

	showMenubar: function() {
		if (!this.isMenubarHidden())
			return;
		$('.main-nav').show();
		if (L.Params.closeButtonEnabled && !window.mode.isTablet()) {
			$('#closebuttonwrapper').show();
		}

		var obj = $('.unfold');
		obj.removeClass('w2ui-icon unfold');
		obj.addClass('w2ui-icon fold');

		this.moveObjectVertically($('#spreadsheet-row-column-frame'), 36);
		this.moveObjectVertically($('#document-container'), 36);
		this.moveObjectVertically($('#presentation-controls-wrapper'), 36);
		this.moveObjectVertically($('#sidebar-dock-wrapper'), 36);
	},

	hideMenubar: function() {
		if (this.isMenubarHidden())
			return;
		$('.main-nav').hide();
		if (L.Params.closeButtonEnabled) {
			$('#closebuttonwrapper').hide();
		}

		var obj = $('.fold');
		obj.removeClass('w2ui-icon fold');
		obj.addClass('w2ui-icon unfold');

		this.moveObjectVertically($('#spreadsheet-row-column-frame'), -36);
		this.moveObjectVertically($('#document-container'), -36);
		this.moveObjectVertically($('#presentation-controls-wrapper'), -36);
		this.moveObjectVertically($('#sidebar-dock-wrapper'), -36);
	},

	isMenubarHidden: function() {
		return $('.main-nav').css('display') === 'none';
	},

	toggleMenubar: function() {
		if (this.isMenubarHidden())
			this.showMenubar();
		else
			this.hideMenubar();
	},

	// Ruler

	showRuler: function() {
		$('.loleaflet-ruler').show();
		$('#map').addClass('hasruler');
		this.setCookie('ShowRulerDefault', true);
	},

	hideRuler: function() {
		$('.loleaflet-ruler').hide();
		$('#map').removeClass('hasruler');
		this.setCookie('ShowRulerDefault', false);
	},

	toggleRuler: function() {
		if (this.isRulerVisible())
			this.hideRuler();
		else
			this.showRuler();
	},

	isRulerVisible: function() {
		return $('.loleaflet-ruler').is(':visible');
	},

	showStatusBar: function() {
		$('#document-container').css('bottom', this.documentBottom);
		$('#sidebar-dock-wrapper').css('bottom', this.sidebarBottom);
		$('#presentation-controls-wrapper').css('bottom', this.presentationControlBottom);
		$('#toolbar-down').show();
		this.setCookie('ShowStatusbarDefault', true);
	},

	hideStatusBar: function() {
		if (!this.isStatusBarVisible())
			return;

		this.documentBottom = $('#document-container').css('bottom');
		this.sidebarBottom = $('#sidebar-dock-wrapper').css('bottom');
		this.presentationControlBottom = $('#presentation-controls-wrapper').css('bottom');
		$('#document-container').css('bottom', '0px');
		$('#sidebar-dock-wrapper').css('bottom', '0px');
		$('#presentation-controls-wrapper').css('bottom','33px');
		$('#toolbar-down').hide();
		this.setCookie('ShowStatusbarDefault', false);
	},

	toggleStatusBar: function() {
		if (this.isStatusBarVisible())
			this.hideStatusBar();
		else
			this.showStatusBar();
	},

	isStatusBarVisible: function() {
		return $('#toolbar-down').is(':visible');
	},

	// Event handlers

	onUpdatePermission: function(e) {
		if (window.mode.isMobile()) {
			if (e.perm === 'edit') {
				$('#toolbar-down').show();
			}
			else {
				$('#toolbar-down').hide();
			}
		}

		// We've resized the document container.
		this.map.invalidateSize();
	},

	// Helper functions

	moveObjectVertically: function(obj, diff) {
		if (obj) {
			var prevTop = obj.css('top');
			if (prevTop) {
				prevTop = parseInt(prevTop.slice(0, -2)) + diff;
			}
			else {
				prevTop = 0 + diff;
			}
			obj.css({'top': String(prevTop) + 'px'});
		}
	},

	setCookie: function(cookie, state) {
		if (!window.initUICookies)
			return;
		var path = '/loleaflet';
		if (window.socketProxy)
			path = window.host + window.serviceRoot + path;
		document.cookie = 'UIDefaults_' + this.map._docLayer._docType + '_' + cookie + '=' + state + '; path=' + path;
	},

	getCookie: function(name) {
		var cookies = document.cookie.split(';');
		for (var i = 0; i < cookies.length; i++) {
			var cookie = cookies[i].trim();
			if (cookie.indexOf('UIDefaults_' + this.map._docLayer._docType + '_' + name) === 0) {
				return cookie.split('=')[1];
			}
		}
		return '';
	}
});

L.control.uiManager = function () {
	return new L.Control.UIManager();
};
