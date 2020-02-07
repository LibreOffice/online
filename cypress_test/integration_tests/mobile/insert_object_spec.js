/* global describe it cy beforeEach require expect afterEach Cypress*/

var helper = require('../common/helper');

describe('Insert objects via insertion wizard.', function() {
	beforeEach(function() {
		helper.beforeAllMobile('empty.odt');

		// Click on edit button
		cy.get('#mobile-edit-button').click();
	});

	afterEach(function() {
		helper.afterAll();
	});

	it('Insert local image.', function() {
		// Open insertion wizard
		cy.get('#tb_actionbar_item_insertion_mobile_wizard')
			.click();

		// We check whether the entry is there
		cy.get('.menu-entry-with-icon')
			.contains('Local Image...');
		// We not not test the insertion, it might depend on the system.
	});

	it('Insert comment.', function() {
		// Open insertion wizard
		cy.get('#tb_actionbar_item_insertion_mobile_wizard')
			.click();

		cy.get('.menu-entry-with-icon')
			.contains('Comment')
			.click();

		// Comment insertion dialog is opened
		cy.get('.loleaflet-annotation-table')
			.should('exist');

		// Push cancel to close the dialog
		cy.get('.vex-dialog-button-secondary.vex-dialog-button.vex-last')
			.click();
	});

	it('Insert default table.', function() {
		// TODO: Select all does not work with core/master
		if (Cypress.env('LO_CORE_VERSION') === 'master')
			return;

		// Open insertion wizard
		cy.get('#tb_actionbar_item_insertion_mobile_wizard')
			.click();

		// Open Table submenu
		cy.get('.menu-entry-with-icon.flex-fullwidth')
			.contains('Table')
			.click();
		cy.get('.mobile-wizard.ui-text')
			.should('be.visible');

		// Push insert table button
		cy.get('.inserttablecontrols button')
			.should('be.visible')
			.click();

		// Table is inserted with the markers shown
		cy.get('.leaflet-marker-icon.table-column-resize-marker')
			.should('exist');

		helper.copyTableToClipboard();

		// Two rows
		cy.get('#copy-paste-container tr')
			.should('have.length', 2);
		// Four cells
		cy.get('#copy-paste-container td')
			.should('have.length', 4);
	});

	it('Insert custom table.', function() {
		// TODO: Select all does not work with core/master
		if (Cypress.env('LO_CORE_VERSION') === 'master')
			return;

		// Open insertion wizard
		cy.get('#tb_actionbar_item_insertion_mobile_wizard')
			.click();

		// Open Table submenu
		cy.get('.menu-entry-with-icon.flex-fullwidth')
			.contains('Table')
			.click();
		cy.get('.mobile-wizard.ui-text')
			.should('be.visible');

		// Change rows and columns
		cy.get('.inserttablecontrols #rows .sinfieldcontrols .plus')
			.click();
		cy.get('.inserttablecontrols #cols .sinfieldcontrols .plus')
			.click();

		// Push insert table button
		cy.get('.inserttablecontrols button')
			.should('be.visible')
			.click();

		// Table is inserted with the markers shown
		cy.get('.leaflet-marker-icon.table-column-resize-marker')
			.should('exist');

		helper.copyTableToClipboard();

		// Three rows
		cy.get('#copy-paste-container tr')
			.should('have.length', 3);
		// Nine cells
		cy.get('#copy-paste-container td')
			.should('have.length', 9);
	});

	it('Insert header.', function() {
		// Get the blinking cursor pos
		cy.get('#document-container').type('xxxx');

		var cursorOrigLeft = 0;
		cy.get('.blinking-cursor')
			.then(function(cursor) {
				expect(cursor).to.have.lengthOf(1) ;
				cursorOrigLeft = cursor[0].getBoundingClientRect().left;
			});

		// Open insertion wizard
		cy.get('#tb_actionbar_item_insertion_mobile_wizard')
			.click();

		// Open header/footer submenu
		cy.get('.sub-menu-title')
			.contains('Header and Footer')
			.click();
		cy.get('.ui-header.level-1.mobile-wizard.ui-widget')
			.should('be.visible');

		// Open header submenu
		cy.get('.ui-header.level-1.mobile-wizard.ui-widget .sub-menu-title')
			.contains('Header')
			.click();

		// Insert header for All
		cy.get('.menu-entry-no-icon')
			.contains('All')
			.click();

		cy.wait(100);

		// Check that the cursor was moved
		cy.get('.blinking-cursor')
			.then(function(cursor) {
				expect(cursor).to.have.lengthOf(1);
				expect(cursor[0].getBoundingClientRect().left).to.be.lessThan(cursorOrigLeft);
			});
	});

	it('Insert footer.', function() {
		// Get the blinking cursor pos
		cy.get('#document-container').type('xxxx');
		var cursorOrigTop = 0;
		cy.get('.blinking-cursor')
			.then(function(cursor) {
				expect(cursor).to.have.lengthOf(1) ;
				cursorOrigTop = cursor[0].getBoundingClientRect().top;
			});

		// Open insertion wizard
		cy.get('#tb_actionbar_item_insertion_mobile_wizard')
			.click();

		// Open header/footer submenu
		cy.get('.sub-menu-title')
			.contains('Header and Footer')
			.click();
		cy.get('.ui-header.level-1.mobile-wizard.ui-widget')
			.should('be.visible');

		// Open footer submenu
		cy.get('.ui-header.level-1.mobile-wizard.ui-widget .sub-menu-title')
			.contains('Footer')
			.click();

		// Insert footer for All
		cy.get('.ui-content.level-1.mobile-wizard[title~="Footer"] .ui-header.level-2.mobile-wizard.ui-widget .menu-entry-no-icon')
			.contains('All')
			.click();

		cy.wait(100);

		// Check that the cursor was moved
		cy.get('.blinking-cursor')
			.then(function(cursor) {
				expect(cursor).to.have.lengthOf(1);
				expect(cursor[0].getBoundingClientRect().top).to.be.greaterThan(cursorOrigTop);
			});
	});

	it('Insert footnote.', function() {
		// Get the blinking cursor pos
		cy.get('#document-container').type('xxxx');
		var cursorOrigTop = 0;
		cy.get('.blinking-cursor')
			.then(function(cursor) {
				expect(cursor).to.have.lengthOf(1);
				cursorOrigTop = cursor[0].getBoundingClientRect().top;
			});

		// Open insertion wizard
		cy.get('#tb_actionbar_item_insertion_mobile_wizard')
			.click();

		// Insert footnote
		cy.get('.menu-entry-with-icon')
			.contains('Footnote')
			.click();

		cy.wait(100);

		// Check that the cursor was moved down
		cy.get('.blinking-cursor')
			.then(function(cursor) {
				expect(cursor).to.have.lengthOf(1);
				expect(cursor[0].getBoundingClientRect().top).to.be.greaterThan(cursorOrigTop);
			});
	});

	it('Insert endnote.', function() {
		// Get the blinking cursor pos
		cy.get('#document-container').type('xxxx');
		var cursorOrigTop = 0;
		cy.get('.blinking-cursor')
			.then(function(cursor) {
				expect(cursor).to.have.lengthOf(1);
				cursorOrigTop = cursor[0].getBoundingClientRect().top;
			});

		// Open insertion wizard
		cy.get('#tb_actionbar_item_insertion_mobile_wizard')
			.click();

		// Insert endnote
		cy.get('.menu-entry-with-icon')
			.contains('Endnote')
			.click();

		cy.wait(100);

		// Check that the cursor was moved down
		cy.get('.blinking-cursor')
			.then(function(cursor) {
				expect(cursor).to.have.lengthOf(1);
				expect(cursor[0].getBoundingClientRect().top).to.be.greaterThan(cursorOrigTop);
			});
	});

	it('Insert page break.', function() {
		// Get the blinking cursor pos
		cy.get('#document-container').type('xxxx');
		var cursorOrigTop = 0;
		cy.get('.blinking-cursor')
			.then(function(cursor) {
				expect(cursor).to.have.lengthOf(1);
				cursorOrigTop = cursor[0].getBoundingClientRect().top;
			});

		// Open insertion wizard
		cy.get('#tb_actionbar_item_insertion_mobile_wizard')
			.click();

		// Insert endnote
		cy.get('.menu-entry-with-icon')
			.contains('Page Break')
			.click();

		cy.wait(100);

		// Check that the cursor was moved down
		cy.get('.blinking-cursor')
			.then(function(cursor) {
				expect(cursor).to.have.lengthOf(1);
				expect(cursor[0].getBoundingClientRect().top).to.be.greaterThan(cursorOrigTop);
			});
	});

	it('Insert column break.', function() {
		// Get the blinking cursor pos
		cy.get('#document-container').type('xxxx');
		var cursorOrigTop = 0;
		cy.get('.blinking-cursor')
			.then(function(cursor) {
				expect(cursor).to.have.lengthOf(1);
				cursorOrigTop = cursor[0].getBoundingClientRect().top;
			});

		// Open insertion wizard
		cy.get('#tb_actionbar_item_insertion_mobile_wizard')
			.click();

		// Do insertion
		cy.get('.menu-entry-with-icon')
			.contains('Column Break')
			.click();

		cy.wait(100);

		// Check that the cursor was moved down
		cy.get('.blinking-cursor')
			.then(function(cursor) {
				expect(cursor).to.have.lengthOf(1);
				expect(cursor[0].getBoundingClientRect().top).to.be.greaterThan(cursorOrigTop);
			});
	});

	it('Insert hyperlink.', function() {
		// Open insertion wizard
		cy.get('#tb_actionbar_item_insertion_mobile_wizard')
			.click();

		// Open hyperlink dialog
		cy.get('.menu-entry-with-icon')
			.contains('Hyperlink...')
			.click();

		// Dialog is opened
		cy.get('.vex-content.hyperlink-dialog')
			.should('exist');

		// Push cancel to close the dialog
		cy.get('.vex-dialog-button-secondary.vex-dialog-button.vex-last')
			.click();
	});

	it('Insert shape.', function() {
		// Open insertion wizard
		cy.get('#tb_actionbar_item_insertion_mobile_wizard')
			.click();

		// Do insertion
		cy.get('.menu-entry-with-icon')
			.contains('Shape')
			.click();

		cy.get('.col.w2ui-icon.basicshapes_rectangle').
			click();

		// Check that the shape is there
		cy.get('.leaflet-pane.leaflet-overlay-pane svg g')
			.should('exist');

		cy.get('.leaflet-pane.leaflet-overlay-pane svg')
			.then(function(svg) {
				expect(svg).to.have.lengthOf(1);
				expect(svg[0].getBBox().width).to.be.greaterThan(0);
				expect(svg[0].getBBox().height).to.be.greaterThan(0);
			});
	});
});
