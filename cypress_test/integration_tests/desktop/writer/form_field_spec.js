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

		// Check also the position relative to the blinking cursor
		cy.get('.blinking-cursor')
			.then(function(cursors) {
				// TODO: why we have two blinking cursors here?
				//expect(cursors).to.have.lengthOf(1);

				var cursorRect = cursors[0].getBoundingClientRect();
				cy.get('.form-field-frame')
					.should(function(frames) {
						expect(frames).to.have.lengthOf(1);
						var frameRect = frames[0].getBoundingClientRect();
						expect(frameRect.top).to.be.lessThan(cursorRect.top);
						expect(frameRect.bottom).to.be.greaterThan(cursorRect.bottom);
						expect(frameRect.left).to.be.lessThan(cursorRect.left);
						expect(frameRect.right).to.be.greaterThan(cursorRect.right);
					});
			});
	}

	function doZoom(zoomIn) {
		helper.initAliasToEmptyString('prevZoom');

		cy.get('#tb_actionbar_item_zoom')
			.invoke('text')
			.as('prevZoom');

		cy.get('@prevZoom')
			.should('not.be.equal', '');

		if (zoomIn) {
			cy.get('.w2ui-tb-image.w2ui-icon.zoomin')
				.click();
		} else {
			cy.get('.w2ui-tb-image.w2ui-icon.zoomout')
				.click();
		}

		cy.get('@prevZoom')
			.then(function(prevZoom) {
				cy.get('#tb_actionbar_item_zoom')
					.should(function(zoomItem) {
						expect(zoomItem.text()).to.be.not.equal(prevZoom);
					});
			});
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

	it('Test drop-down field with no selection.', function() {
		helper.loadTestDoc('drop_down_form_field_noselection.odt', 'writer');

		// Move the cursor next to the form field
		cy.get('textarea.clipboard')
			.type('{rightArrow}');

		buttonShouldExist();

		cy.get('.drop-down-field-list-item.selected')
			.should('not.exist');
	});

	it('Test drop-down field with no items.', function() {
		helper.loadTestDoc('drop_down_form_field_noitem.odt', 'writer');

		// Move the cursor next to the form field
		cy.get('textarea.clipboard')
			.type('{rightArrow}');

		buttonShouldExist();

		cy.get('.drop-down-field-list-item')
			.should('have.text', 'No Item specified');

		cy.get('.drop-down-field-list-item.selected')
			.should('not.exist');

		cy.get('.form-field-button')
			.click();

		cy.get('.drop-down-field-list')
			.should('be.visible');

		cy.contains('.drop-down-field-list-item', 'No Item specified')
			.click();

		cy.get('.drop-down-field-list-item.selected')
			.should('not.exist');
	});

	it('Test field button after zoom.', function() {
		helper.loadTestDoc('form_field.odt', 'writer');

		// Move the cursor next to the form field
		cy.get('textarea.clipboard')
			.type('{rightArrow}');

		buttonShouldExist();

		// Do a zoom in
		doZoom(true);

		buttonShouldExist();

		// Do a zoom out
		doZoom(false);

		buttonShouldExist();

		// Now check that event listener does not do
		// anything stupid after the button is removed.

		// Move the cursor away from the field
		cy.get('textarea.clipboard')
			.type('{leftArrow}');

		buttonShouldNotExist();

		// Do a zoom in again
		doZoom(true);
	});

	it('Test dynamic font size.', function() {
		helper.loadTestDoc('form_field.odt', 'writer');

		// Move the cursor next to the form field
		cy.get('textarea.clipboard')
			.type('{rightArrow}');

		buttonShouldExist();

		// Get the initial font size from the style
		helper.initAliasToEmptyString('prevFontSize');

		cy.get('.drop-down-field-list-item')
			.invoke('css', 'font-size')
			.as('prevFontSize');

		cy.get('@prevFontSize')
			.should('not.be.equal', '');

		// Do a zoom in
		doZoom(true);

		buttonShouldExist();

		// Check that the font size was changed
		cy.get('@prevFontSize')
			.then(function(prevFontSize) {
				cy.get('.drop-down-field-list-item')
					.should(function(items) {
						var prevSize = parseInt(prevFontSize, 10);
						var currentSize = parseInt(items.css('font-size'), 10);
						expect(currentSize).to.be.greaterThan(prevSize);
					});
			});

		cy.get('.drop-down-field-list-item')
			.invoke('css', 'font-size')
			.as('prevFontSize');

		// Do a zoom out
		doZoom(false);

		buttonShouldExist();

		// Check that the font size was changed
		cy.get('@prevFontSize')
			.then(function(prevFontSize) {
				cy.get('.drop-down-field-list-item')
					.should(function(items) {
						var prevSize = parseInt(prevFontSize, 10);
						var currentSize = parseInt(items.css('font-size'), 10);
						expect(currentSize).to.be.lessThan(prevSize);
					});
			});
	});
});

