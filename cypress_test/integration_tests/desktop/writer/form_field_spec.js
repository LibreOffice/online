/* global describe it cy require afterEach expect */

var helper = require('../../common/helper');

describe('Form field button tests.', function() {

	afterEach(function() {
		helper.afterAll('form_field.odt', 'writer');
	});

	function buttonShouldNotExist() {
		cy.get('.form-field-frame')
			.should('not.exist');

		cy.get('.form-field-button')
			.should('not.exist');

		cy.get('.drop-down-field-list')
			.should('not.exist');
	}

	function buttonShouldExist() {
		cy.get('.form-field-frame')
			.should('exist');

		cy.get('.form-field-button')
			.should('exist');

		cy.get('.drop-down-field-list')
			.should('exist');
	}

	it('Activate and deactivate form field button.', function() {
		helper.loadTestDoc('form_field.odt', 'writer');

		// We don't have the button by default
		buttonShouldNotExist();

		// Move the cursor next to the form field
		cy.get('textarea.clipboard')
			.type('{rightArrow}');

		buttonShouldExist();

		// Move the cursor again to the other side of the field
		cy.get('textarea.clipboard')
			.type('{rightArrow}');

		buttonShouldExist();

		// Move the cursor away
		cy.get('textarea.clipboard')
			.type('{rightArrow}');

		buttonShouldNotExist();

		// Move the cursor back next to the field
		cy.get('textarea.clipboard')
			.type('{leftArrow}');

		buttonShouldExist();
	});

	it('Check drop down list.', function() {
		helper.loadTestDoc('form_field.odt', 'writer');

		// Move the cursor next to the form field
		cy.get('textarea.clipboard')
			.type('{rightArrow}');

		buttonShouldExist();

		cy.get('.drop-down-field-list')
			.should('not.be.visible');

		// Check content of the list
		cy.get('.drop-down-field-list')
			.should(function(list) {
				expect(list[0].children.length).to.be.equal(4);
				expect(list[0].children[0]).to.have.text('February');
				expect(list[0].children[1]).to.have.text('January');
				expect(list[0].children[2]).to.have.text('December');
				expect(list[0].children[3]).to.have.text('July');
			});

		cy.get('.drop-down-field-list-item.selected')
			.should('have.text', 'February');

		// Select a new item
		cy.get('.form-field-button')
			.click();

		cy.get('.drop-down-field-list')
			.should('be.visible');

		cy.contains('.drop-down-field-list-item', 'July')
			.click();

		// List is hidden, but have the right selected element
		cy.get('.drop-down-field-list')
			.should('not.be.visible');

		cy.get('.drop-down-field-list-item.selected')
			.should('have.text', 'July');
	});

	it('Test field editing', function() {
		helper.loadTestDoc('form_field.odt', 'writer');

		// Move the cursor next to the form field
		cy.get('textarea.clipboard')
			.type('{rightArrow}');

		// Select a new item
		cy.get('.form-field-button')
			.click();

		cy.get('.drop-down-field-list')
			.should('be.visible');

		cy.contains('.drop-down-field-list-item', 'January')
			.click();

		// Move the cursor away and back
		cy.get('textarea.clipboard')
			.type('{leftArrow}');

		buttonShouldNotExist();

		// Move the cursor back next to the field
		cy.get('textarea.clipboard')
			.type('{rightArrow}');

		buttonShouldExist();

		cy.get('.drop-down-field-list-item.selected')
			.should('have.text', 'January');

		// Do the same from the right side of the field.
		cy.get('textarea.clipboard')
			.type('{rightArrow}');

		buttonShouldExist();

		// Select a new item
		cy.get('.form-field-button')
			.click();

		cy.get('.drop-down-field-list')
			.should('be.visible');

		cy.contains('.drop-down-field-list-item', 'December')
			.click();

		cy.get('textarea.clipboard')
			.type('{rightArrow}');

		buttonShouldNotExist();

		// Move the cursor back next to the field
		cy.get('textarea.clipboard')
			.type('{leftArrow}');

		buttonShouldExist();

		cy.get('.drop-down-field-list-item.selected')
			.should('have.text', 'December');
	});

	it('Multiple form field button activation.', function() {
		helper.loadTestDoc('multiple_form_fields.odt', 'writer');

		// We don't have the button by default
		buttonShouldNotExist();

		// Move the cursor next to the first form field
		cy.get('textarea.clipboard')
			.type('{rightArrow}');

		buttonShouldExist();

		// Move the cursor to the other side of the field
		cy.get('textarea.clipboard')
			.type('{rightArrow}');

		buttonShouldExist();

		// Move the cursor to the second form field
		cy.get('textarea.clipboard')
			.type('{rightArrow}');

		buttonShouldExist();

		// Move the cursor to the other side of the second field
		cy.get('textarea.clipboard')
			.type('{rightArrow}');

		buttonShouldExist();

		// Move the cursor away of the second field
		cy.get('textarea.clipboard')
			.type('{rightArrow}');

		buttonShouldNotExist();
	});
});

