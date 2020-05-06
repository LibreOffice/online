/* global describe it cy beforeEach require afterEach */

var helper = require('../../common/helper');
var mobileHelper = require('../../common/mobile_helper');
var writerMobileHelper = require('./writer_mobile_helper');

describe('Trigger hamburger menu options.', function() {
	var testFileName = 'hamburger_menu.odt';

	beforeEach(function() {
		mobileHelper.beforeAllMobile(testFileName, 'writer');

		// Click on edit button
		mobileHelper.enableEditingMobile();
	});

	afterEach(function() {
		helper.afterAll(testFileName);
	});

	function hideText() {
		// Change text color to white to hide text.
		writerMobileHelper.selectAllMobile();

		mobileHelper.openMobileWizard();

		cy.get('#FontColor')
			.click();

		mobileHelper.selectFromColorPalette(0, 0, 7);

		// End remove spell checking red lines
		mobileHelper.openHamburgerMenu();

		cy.contains('.menu-entry-with-icon', 'View')
			.click();

		cy.contains('.menu-entry-with-icon', 'Automatic Spell Checking')
			.click();
	}

	function openPageWizard() {
		mobileHelper.openHamburgerMenu();

		cy.contains('.menu-entry-with-icon', 'Page Setup')
			.click();

		cy.get('#mobile-wizard-content')
			.should('not.be.empty');
	}

	function closePageWizard() {
		cy.get('#mobile-wizard-back')
			.click();

		cy.get('#mobile-wizard')
			.should('not.be.visible');
	}


	it('Search some word.', function() {
		mobileHelper.openHamburgerMenu();

		cy.contains('.menu-entry-with-icon', 'Search')
			.click();

		// Search bar become visible
		cy.get('#mobile-wizard')
			.should('be.visible');

		// Search for some word
		cy.get('input#searchterm')
			.type('a');

		cy.get('input#searchterm')
			.should('have.prop', 'value', 'a');

		cy.get('#search')
			.click();

		// Part of the text should be selected
		cy.get('.leaflet-marker-icon')
			.should('exist');

		cy.get('#copy-paste-container p')
			.should('have.text', '\na');

		cy.get('#copy-paste-container p b')
			.should('not.exist');

		// Go for the second match
		cy.get('#search')
			.click();

		cy.get('#copy-paste-container p b')
			.should('exist');

		cy.get('#copy-paste-container p')
			.should('have.text', '\na');

		// Go back to the first match
		cy.get('#backsearch')
			.click();

		cy.get('#copy-paste-container p b')
			.should('not.exist');

		cy.get('#copy-paste-container p')
			.should('have.text', '\na');
	});

	it('Check word counts.', function() {
		writerMobileHelper.selectAllMobile();

		mobileHelper.openHamburgerMenu();

		cy.contains('.menu-entry-with-icon', 'Word Count...')
			.click();

		// Selected counts
		cy.get('#selectwords')
			.should('have.text', '106');

		cy.get('#selectchars')
			.should('have.text', '1,174');

		cy.get('#selectcharsnospaces')
			.should('have.text', '1,069');

		cy.get('#selectcjkchars')
			.should('have.text', '0');

		// General counts
		cy.get('#docwords')
			.should('have.text', '106');

		cy.get('#docchars')
			.should('have.text', '1,174');

		cy.get('#doccharsnospaces')
			.should('have.text', '1,069');

		cy.get('#doccjkchars')
			.should('have.text', '0');
	});

	it('Page setup: change paper size.', function() {
		var centerTile = '.leaflet-tile-loaded[style=\'width: 256px; height: 256px; left: 255px; top: 517px;\']';
		helper.imageShouldBeFullWhiteOrNot(centerTile, true);

		openPageWizard();

		cy.get('#papersize')
			.click();

		cy.contains('.ui-combobox-text', 'C6 Envelope')
			.click();

		// Smaller paper size makes center tile to contain text too.
		helper.imageShouldBeFullWhiteOrNot(centerTile, false);

		// Check that the page wizard shows the right value after reopen.
		closePageWizard();

		openPageWizard();

		cy.get('#papersize .ui-header-left')
			.should('have.text', 'C6 Envelope');
	});

	it('Page setup: change paper width.', function() {
		var centerTile = '.leaflet-tile-loaded[style=\'width: 256px; height: 256px; left: 255px; top: 517px;\']';
		helper.imageShouldBeFullWhiteOrNot(centerTile, true);

		openPageWizard();

		cy.get('#paperwidth .spinfield')
			.clear()
			.type('5')
			.type('{enter}');

		// Smaller paper size makes center tile to contain text too.
		helper.imageShouldBeFullWhiteOrNot(centerTile, false);

		// Check that the page wizard shows the right value after reopen.
		closePageWizard();

		openPageWizard();

		cy.get('#papersize .ui-header-left')
			.should('have.text', 'User');

		cy.get('#paperwidth .spinfield')
			.should('have.attr', 'value', '5');
	});

	it('Page setup: change paper height.', function() {
		var centerTile = '.leaflet-tile-loaded[style=\'width: 256px; height: 256px; left: 255px; top: 517px;\']';
		helper.imageShouldBeFullWhiteOrNot(centerTile, true);

		openPageWizard();

		cy.get('#paperheight .spinfield')
			.clear()
			.type('3.0')
			.type('{enter}');

		// Smaller paper size makes center tile to contain the end of the page.
		helper.imageShouldBeFullWhiteOrNot(centerTile, false);

		// Check that the page wizard shows the right value after reopen.
		closePageWizard();

		openPageWizard();

		cy.get('#papersize .ui-header-left')
			.should('have.text', 'User');

		cy.get('#paperheight .spinfield')
			.should('have.attr', 'value', '3');
	});

	it('Page setup: change orientation.', function() {
		cy.get('.leaflet-tile-loaded[style=\'width: 256px; height: 256px; left: 1023px; top: 5px;\']')
			.should('not.exist');

		openPageWizard();

		cy.get('#paperorientation')
			.click();

		cy.contains('.ui-combobox-text', 'Landscape')
			.click();

		// We got some extra tiles horizontally.
		cy.get('.leaflet-tile-loaded[style=\'width: 256px; height: 256px; left: 1023px; top: 5px;\']')
			.should('exist');

		// Check that the page wizard shows the right value after reopen.
		closePageWizard();

		openPageWizard();

		cy.get('#paperorientation .ui-header-left')
			.should('have.text', 'Landscape');
	});

	it('Page setup: change margin.', function() {
		var centerTile = '.leaflet-tile-loaded[style=\'width: 256px; height: 256px; left: 255px; top: 261px;\']';
		helper.imageShouldBeFullWhiteOrNot(centerTile, false);

		openPageWizard();

		cy.get('#marginLB')
			.click();

		cy.contains('.ui-combobox-text', 'None')
			.click();

		// Text is moved up by margin removal, so the the center tile will be empty.
		helper.imageShouldBeFullWhiteOrNot(centerTile, true);

		// Check that the page wizard shows the right value after reopen.
		closePageWizard();

		openPageWizard();

		cy.get('#marginLB .ui-header-left')
			.should('have.text', 'None');
	});

	it('Show formatting marks.', function() {
		// Hide text so the center tile is full white.
		hideText();

		var centerTile = '.leaflet-tile-loaded[style=\'width: 256px; height: 256px; left: 255px; top: 261px;\']';
		helper.imageShouldBeFullWhiteOrNot(centerTile, true);

		// Enable it first -> spaces will be visible.
		mobileHelper.openHamburgerMenu();

		cy.contains('.menu-entry-with-icon', 'View')
			.click();

		cy.contains('.menu-entry-with-icon', 'Formatting Marks')
			.click();

		helper.imageShouldBeFullWhiteOrNot(centerTile, false);

		// Then disable it again.
		mobileHelper.openHamburgerMenu();

		cy.contains('.menu-entry-with-icon', 'View')
			.click();

		cy.contains('.menu-entry-with-icon', 'Formatting Marks')
			.click();

		helper.imageShouldBeFullWhiteOrNot(centerTile, true);
	});

	it('Automatic spell checking.', function() {
		// Hide text so the center tile is full white.
		hideText();

		var centerTile = '.leaflet-tile-loaded[style=\'width: 256px; height: 256px; left: 255px; top: 261px;\']';
		helper.imageShouldBeFullWhiteOrNot(centerTile, true);

		// Enable it first.
		mobileHelper.openHamburgerMenu();

		cy.contains('.menu-entry-with-icon', 'View')
			.click();

		cy.contains('.menu-entry-with-icon', 'Automatic Spell Checking')
			.click();

		helper.imageShouldBeFullWhiteOrNot(centerTile, false);

		// Then disable it again.
		mobileHelper.openHamburgerMenu();

		cy.contains('.menu-entry-with-icon', 'View')
			.click();

		cy.contains('.menu-entry-with-icon', 'Automatic Spell Checking')
			.click();

		helper.imageShouldBeFullWhiteOrNot(centerTile, true);
	});

	it('Resolved comments.', function() {
		// Insert comment first
		mobileHelper.openInsertionWizard();

		cy.contains('.menu-entry-with-icon', 'Comment')
			.click();

		cy.get('.loleaflet-annotation-table')
			.should('exist');

		cy.get('.loleaflet-annotation-textarea')
			.type('some text');

		cy.get('.vex-dialog-button-primary')
			.click();

		cy.get('.loleaflet-annotation:nth-of-type(2)')
			.should('have.attr', 'style')
			.should('not.contain', 'visibility: hidden');

		// Resolve comment
		cy.get('.loleaflet-annotation-menu')
			.click({force: true});

		cy.contains('.context-menu-link', 'Resolve')
			.click();

		cy.get('.loleaflet-annotation:nth-of-type(2)')
			.should('have.attr', 'style')
			.should('contain', 'visibility: hidden');

		// Show resolved comments
		mobileHelper.openHamburgerMenu();

		cy.contains('.menu-entry-with-icon', 'View')
			.click();

		cy.contains('.menu-entry-with-icon', 'Resolved Comments')
			.click();

		cy.get('.loleaflet-annotation:nth-of-type(2)')
			.should('have.attr', 'style')
			.should('not.contain', 'visibility: hidden');

		// Hide resolved comments
		mobileHelper.openHamburgerMenu();

		cy.contains('.menu-entry-with-icon', 'View')
			.click();

		cy.contains('.menu-entry-with-icon', 'Resolved Comments')
			.click();

		// TODO: can't hide resolved comments again.
		//cy.get('.loleaflet-annotation:nth-of-type(2)')
		//	.should('have.attr', 'style')
		//	.should('contain', 'visibility: hidden');
	});

	it('Check version information.', function() {
		mobileHelper.openHamburgerMenu();

		// Open about dialog
		cy.contains('.menu-entry-with-icon', 'About')
			.click();

		cy.get('.vex-content')
			.should('exist');

		// Check the version
		if (helper.getLOVersion() === 'master') {
			cy.contains('#lokit-version', 'LibreOffice')
				.should('exist');
		} else if (helper.getLOVersion() === 'cp-6-2' ||
				   helper.getLOVersion() === 'cp-6-4')
		{
			cy.contains('#lokit-version', 'Collabora Office')
				.should('exist');
		}

		// Close about dialog
		cy.get('.vex-close')
			.click({force : true});

		cy.get('.vex-content')
			.should('not.exist');
	});
});

