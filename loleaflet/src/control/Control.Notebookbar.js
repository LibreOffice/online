/* -*- js-indent-level: 8 -*- */
/*
 * L.Control.Notebookbar
 */

/* global $ */
L.Control.Notebookbar = L.Control.extend({
	options: {
		docType: ''
	},

	onAdd: function (map) {
		this.map = map;

		this.loadTab(this.getHomeTab());
	},

	clearNotebookbar: function() {
		$('.root-container.notebookbar').remove();
		$('.ui-tabs.notebookbar').remove();
	},

	loadTab: function(tabJSON) {
		this.clearNotebookbar();
		tabJSON = JSON.parse(tabJSON);
		var builder = new L.control.notebookbarBuilder({mobileWizard: this, map: this.map, cssClass: 'notebookbar'});
		builder.build($('#toolbar-wrapper').get(0), [tabJSON]);
	},

	setTabs: function(tabs) {
		$('nav').prepend(tabs);
	},

	selectedTab: function(tabText) {
		switch (tabText) {
		case 'Home':
			this.loadTab(this.getHomeTab());
			break;

		case 'Insert':
			this.loadTab(this.getInsertTab());
			break;

		case 'Layout':
			this.loadTab(this.getLayoutTab());
			break;
		
		case 'References':
			this.loadTab(this.getReferencesTab());
			break;

		case 'Table':
			this.loadTab(this.getTableTab());
			break;
		}
	},

	// required, called by builder, not needed in this container
	setCurrentScrollPosition: function() {},

	getHomeTab: function() {
		return '';
	},

	getInsertTab: function() {
		return '';
	},

	getLayoutTab: function() {
		return '';
    },

	getReferencesTab: function() {
		return '';
	}
});

L.control.notebookbar = function (options) {
	return new L.Control.Notebookbar(options);
};
