/* global describe it cy beforeEach require afterEach expect*/

var helper = require('../common/helper');

describe('Apply/modify styles.', function() {
	beforeEach(function() {
		helper.beforeAllMobile('simple.odt');

		// Click on edit button
		cy.get('#mobile-edit-button').click();
	});

	afterEach(function() {
		helper.afterAll();
	});

	function applyStyle(styleName) {
		// Do a new selection
		helper.selectAllMobile();

		// Open mobile wizard
		cy.get('#tb_actionbar_item_mobile_wizard')
			.should('not.have.class', 'disabled')
			.click();

		// Change font name
		cy.get('#applystyle')
			.click();

		cy.wait(200);

		cy.get('#mobile-wizard-back')
			.should('be.visible');

		cy.get('.mobile-wizard.ui-combobox-text')
			.contains(styleName)
			.scrollIntoView();

		cy.wait(200);

		cy.get('.mobile-wizard.ui-combobox-text')
			.contains(styleName)
			.click();

		// Combobox entry contains the selected font name
		if (styleName !== 'Clear formatting') {
			cy.get('#applystyle .ui-header-right .entry-value')
				.contains(styleName);
		}

		// Close mobile wizard
		cy.get('#tb_actionbar_item_mobile_wizard')
			.click();
	}

	it('Apply new style.', function() {
		// Apply Title style
		applyStyle('Title');

		helper.copyTextToClipboard();

		cy.get('#copy-paste-container p font')
			.should('have.attr', 'face', 'Liberation Sans, sans-serif');
		cy.get('#copy-paste-container p font font')
			.should('have.attr', 'style', 'font-size: 28pt');
	});

	it('Clear style.', function() {
		// Apply Title style
		applyStyle('Title');

		helper.copyTextToClipboard();

		cy.get('#copy-paste-container p font')
			.should('have.attr', 'face', 'Liberation Sans, sans-serif');
		cy.get('#copy-paste-container p font font')
			.should('have.attr', 'style', 'font-size: 28pt');

		helper.selectAllMobile();

		var selectionOrigLeft = 0;
		var selectionOrigRight = 0;
		cy.get('.leaflet-marker-icon')
			.then(function(marker) {
				expect(marker).to.have.lengthOf(2);
				selectionOrigLeft = marker[0].getBoundingClientRect().left;
				selectionOrigRight = marker[1].getBoundingClientRect().right;
			});

		// Clear formatting
		applyStyle('Clear formatting');

		helper.selectAllMobile();

		cy.get('.leaflet-marker-icon')
			.then(function(marker) {
				expect(marker).to.have.lengthOf(2);
				expect(marker[0].getBoundingClientRect().left).to.be.lessThan(selectionOrigLeft);
				expect(marker[1].getBoundingClientRect().right).to.be.lessThan(selectionOrigRight);
				expect(marker[1].getBoundingClientRect().right - marker[0].getBoundingClientRect().left)
					.to.be.lessThan(selectionOrigRight - selectionOrigLeft);
			});
	});

	it('Modify existing style.', function() {
		// Apply Title style
		applyStyle('Title');

		helper.copyTextToClipboard();

		cy.get('#copy-paste-container p font')
			.should('have.attr', 'face', 'Liberation Sans, sans-serif');
		cy.get('#copy-paste-container p font font')
			.should('have.attr', 'style', 'font-size: 28pt');

		// Open mobile wizard
		cy.get('#tb_actionbar_item_mobile_wizard')
			.should('not.have.class', 'disabled')
			.click();

		// Apply italic
		cy.get('#Italic')
			.click();

		// Close mobile wizard
		cy.get('#tb_actionbar_item_mobile_wizard')
			.click();

		helper.copyTextToClipboard();

		cy.get('#copy-paste-container p i')
			.should('exist');

		// Open mobile wizard
		cy.get('#tb_actionbar_item_mobile_wizard')
			.should('not.have.class', 'disabled')
			.click();

		cy.get('#StyleUpdateByExample')
			.click();
	});

	it('New style item is hidden.', function() {
		// New style item opens a tunneled dialog
		// what we try to avoid.

		// Open mobile wizard
		cy.get('#tb_actionbar_item_mobile_wizard')
			.click();

		cy.get('#StyleUpdateByExample')
			.should('exist');

		cy.get('#StyleNewByExample')
			.should('not.exist');
	});
});
