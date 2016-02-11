/*
 * L.Control.Tabs is used to swtich sheets in Calc
 */

/* global $ */
L.Control.Tabs = L.Control.extend({
	onAdd: function (map) {
		this._tabsInitialized = false;
		this._spreadsheetTabs = {};
		var docContainer = map.options.documentContainer;
		this._tabsCont = L.DomUtil.create('div', 'spreadsheet-tab', docContainer.parentElement);

		$.contextMenu({
			selector: '.spreadsheet-context-menu',
			callback: function(key, options) {
				var nPos = parseInt(options.$trigger.attr('id').split('spreadsheet-tab')[1]);

				if (key === 'insertsheetbefore')
					map.insertPage(nPos);
				if (key === 'insertsheetafter')
					map.insertPage(nPos + 1);
			},
			items: {
				'insertsheetbefore': {name: 'Insert sheet before this'},
				'insertsheetafter': {name: 'Insert sheet after this'},
				'deletesheet': {name: 'Delete sheet',
						callback: function(key, options) {
							var nPos = parseInt(options.$trigger.attr('id').split('spreadsheet-tab')[1]);
							vex.dialog.confirm({
								message: 'Are you sure you want to delete this sheet?',
								callback: function(data) {
									if (data)
										map.deletePage(nPos);
								}
							});
						}
				 },
				'renamesheet': {name: 'Rename sheet',
							callback: function(key, options){
							var nPos = parseInt(options.$trigger.attr('id').split('spreadsheet-tab')[1]);
							vex.dialog.open({
								message: 'Enter new sheet name',
								input: '<input name="sheetname" type="text" required />',
								callback: function(data){
									map.renamePage(data.sheetname, nPos);
								}
							});
						}}
			},
			zIndex: 10
		});

		map.on('updateparts', this._updateDisabled, this);
	},

	_updateDisabled: function (e) {
		var parts = e.parts;
		var selectedPart = e.selectedPart;
		var docType = e.docType;
		if (docType === 'text') {
			return;
		}
		if (docType === 'spreadsheet') {
			if (!this._tabsInitialized) {
				// make room for the preview
				var docContainer = this._map.options.documentContainer;
				L.DomUtil.addClass(docContainer, 'spreadsheet-document');
				setTimeout(L.bind(function () {
					this._map.invalidateSize();
					$('.scroll-container').mCustomScrollbar('update');
					$('.scroll-container').mCustomScrollbar('scrollTo', [0, 0]);
				}, this), 100);
				this._tabsInitialized = true;
			}
			if ('partNames' in e) {
				while (this._tabsCont.firstChild) {
					this._tabsCont.removeChild(this._tabsCont.firstChild);
				}
				for (var i = 0; i < parts; i++) {
					var id = 'spreadsheet-tab' + i;
					var tab = L.DomUtil.create('li', 'spreadsheet-context-menu', this._tabsCont);
					tab.innerHTML = e.partNames[i];
					tab.id = id;

					L.DomEvent
						.on(tab, 'click', L.DomEvent.stopPropagation)
						.on(tab, 'click', L.DomEvent.stop)
						.on(tab, 'click', this._setPart, this)
						.on(tab, 'click', this._refocusOnMap, this);
					this._spreadsheetTabs[id] = tab;
				}
			}
			for (var key in this._spreadsheetTabs) {
				var part =  parseInt(key.match(/\d+/g)[0]);
				L.DomUtil.removeClass(this._spreadsheetTabs[key], 'selected');
				if (part === selectedPart) {
					L.DomUtil.addClass(this._spreadsheetTabs[key], 'selected');
				}
			}
		}
	},

	_setPart: function (e) {
		var part =  e.target.id.match(/\d+/g)[0];
		if (part !== null) {
			this._map.setPart(parseInt(part));
		}
	}
});

L.control.tabs = function (options) {
	return new L.Control.Tabs(options);
};
