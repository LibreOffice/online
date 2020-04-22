/* global describe it cy beforeEach require afterEach expect */

var helper = require('../../common/helper');
var mobileHelper = require('../../common/mobile_helper');
var impress = require('../../common/impress');

describe('Impress focus tests', function() {
	beforeEach(function() {
		mobileHelper.beforeAllMobile('focus.odp', 'impress');
	});

	afterEach(function() {
		helper.afterAll('focus.odp');
	});

	it('Select text box, no editing', function() {

		mobileHelper.enableEditingMobile();

		impress.assertNotInTextEditMode();

		// Body has the focus -> can't type in the document
		cy.document().its('activeElement.tagName')
			.should('be.eq', 'BODY');

		// One tap on a text shape, on the whitespace area,
		// does not start editing.
		cy.get('#document-container')
			.then(function(items) {
				expect(items).have.length(1);

				// Click in the left-bottom corner where there is no text.
				let posX = items[0].getBoundingClientRect().left + items[0].getBoundingClientRect().width / 4;
				let posY = items[0].getBoundingClientRect().top + items[0].getBoundingClientRect().height / 2;
				cy.log('Got left-bottom quantile at (' + posX + ', ' + posY + ')');

				cy.get('#document-container')
					.click(posX, posY);
			});

		// No focus
		cy.document().its('activeElement.tagName')
			.should('be.eq', 'BODY');

		// Shape selection.
		cy.get('.leaflet-pane.leaflet-overlay-pane svg g')
			.should('exist');

		// But no editing.
		impress.assertNotInTextEditMode();
	});

	it('Double-click to edit', function() {

		mobileHelper.enableEditingMobile();

		impress.assertNotInTextEditMode();

		// Enter edit mode by double-clicking.
		cy.get('#document-container')
			.dblclick();

		impress.typeTextAndVerify('Hello Impress');

		// End editing.
		cy.get('#document-container')
			.type('{esc}');

		impress.assertNotInTextEditMode();

		// Enter edit mode by double-clicking again.
		cy.get('#document-container')
			.dblclick();

		// Clear the text.
		helper.clearAllText();

		impress.typeTextAndVerify('Bazinga Impress');
	});

	it('Single-click to edit', function() {

		mobileHelper.enableEditingMobile();

		impress.assertNotInTextEditMode();

		cy.get('#document-container')
			.then(function(items) {
				expect(items).have.length(1);

				// Click in the top left corner where there is no text.
				let posX = items[0].getBoundingClientRect().width / 2;
				let posY = items[0].getBoundingClientRect().height / 2;
				cy.log('Got center coordinates at (' + posX + ', ' + posY + ')');

				// Start editing; click on the text.
				cy.get('#document-container')
					.click(posX, posY);

				impress.typeTextAndVerify('Hello Impress');

				// End editing.
				cy.get('#document-container')
					.type('{esc}');

				impress.assertNotInTextEditMode();

				// Single-click to re-edit.
				cy.get('#document-container')
					.then(function(items) {
						expect(items).have.length(1);

						cy.get('#document-container')
							.click(posX, posY);

						impress.assertInTextEditMode();

						// Clear the text.
						helper.clearAllText();

						impress.typeTextAndVerify('Bazinga Impress');

						// End editing.
						cy.get('#document-container')
							.type('{esc}');

						impress.assertNotInTextEditMode();
					});
			});
	});
});
