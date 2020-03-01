/* global describe it cy beforeEach require afterEach expect */

var helper = require('../../common/helper');
var impress = require('../../common/impress');

describe('Impress focus tests', function() {
	beforeEach(function() {
		helper.beforeAllMobile('focus.odp', 'impress');
	});

	afterEach(function() {
		helper.afterAll('focus.odp');
	});

	it('Select text box, no editing', function() {

		helper.enableEditingMobile();

		impress.assertNotInTextEditMode();

		cy.get('#tb_actionbar_item_mobile_wizard')
			.should('not.have.class', 'disabled');

		// Body has the focus -> can't type in the document.
		cy.document().its('activeElement.tagName')
			.should('be.eq', 'BODY');

		// One tap on a text shape, on the whitespace area,
		// does not start editing.
		cy.get('#document-container')
			.then(function(items) {
				expect(items).have.length(1);

				// Click in the top left corner where there is no text.
				let posX = items[0].getBoundingClientRect().left + items[0].getBoundingClientRect().width / 4;
				let posY = items[0].getBoundingClientRect().top + items[0].getBoundingClientRect().height / 4;
				cy.log('Got first quartile at (' + posX + ', ' + posY + ')');

				cy.get('#document-container')
					.click(posX, posY);
			});

		// No focus.
		cy.document().its('activeElement.tagName')
			.should('be.eq', 'BODY');

		// Shape selection.
		cy.get('.leaflet-pane.leaflet-overlay-pane svg g')
			.should('exist');

		// But no editing.
		impress.assertNotInTextEditMode();
	});

	it('Single- and Double-click to edit', function() {

		helper.enableEditingMobile();

		impress.assertNotInTextEditMode();

		// Enter edit mode by double-clicking.
		cy.get('#document-container')
			.dblclick();

		impress.typeTextAndVerify('Hello Impress');

		// Now test single-click editing, by ending the previous
		// editing and single-click on the text we entered.
		cy.get('.leaflet-marker-icon')
			.then(function(marker) {
				// Get the center coordinates for single-click.
				expect(marker).to.have.lengthOf(2);
				let posX = marker[0].getBoundingClientRect().right +
					(marker[1].getBoundingClientRect().left - marker[0].getBoundingClientRect().right) / 2;
				let posY = marker[0].getBoundingClientRect().top - marker[0].getBoundingClientRect().height;
				cy.log('Got text center at (' + posX + ', ' + posY + ')');

				// Clear the text.
				helper.clearAllText();

				// End editing.
				cy.get('#document-container')
					.type('{esc}').wait(500);

				impress.assertNotInTextEditMode();

				// Single-click to re-edit.
				cy.get('#document-container')
					.then(function(items) {
						expect(items).have.length(1);

						cy.get('#document-container')
							.click(posX, posY).wait(500);

						impress.assertInTextEditMode();

						helper.selectAllText();

						impress.typeTextAndVerify('Bazinga Impress');
					});
			});

	});
});
