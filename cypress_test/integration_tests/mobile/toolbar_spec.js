/* global describe it cy Cypress beforeEach*/

describe('Toolbar tests', function() {
	beforeEach(function() {
		// Get a clean test document
		cy.task('copyFile', {
			sourceDir: Cypress.env('DATA_FOLDER'),
			destDir: Cypress.env('WORKDIR'),
			fileName: 'empty.odt',
		});

		// Open test document
		cy.viewport('iphone-3')
		cy.visit('http://localhost:9980/loleaflet/fc04ba550/loleaflet.html?file_path=file://' +
			Cypress.env('WORKDIR') + 'empty.odt')

		// Wait for the document to fully load
		cy.get('.leaflet-tile-loaded')
	});

	it('State of mobile wizard toolbar item.', function() {
		// Mobile wizard toolbar button is disabled by default
		cy.get('#tb_actionbar_item_mobile_wizard')
			.should('have.class', 'disabled')

		// Click on edit button
		cy.get('#mobile-edit-button').click()

		// Button should be enabled now
		cy.get('#tb_actionbar_item_mobile_wizard')
			.should('not.have.class', 'disabled')
	})

	it('State of insertion mobile wizard toolbar item.', function() {
		// Insertion mobile wizard toolbar button is disabled by default
		cy.get('#tb_actionbar_item_insertion_mobile_wizard')
			.should('have.class', 'disabled')

		// Click on edit button
		cy.get('#mobile-edit-button').click()

		// Button should be enabled now
		cy.get('#tb_actionbar_item_insertion_mobile_wizard')
			.should('not.have.class', 'disabled')
	})

	it('State of insert comment toolbar item.', function() {
		// Insertion mobile wizard toolbar button is disabled by default
		cy.get('#tb_actionbar_item_insertcomment')
			.should('have.class', 'disabled')

		// Click on edit button
		cy.get('#mobile-edit-button').click()

		// Button should be enabled now
		cy.get('#tb_actionbar_item_insertcomment')
			.should('not.have.class', 'disabled')
	})

	it('State of undo toolbar item.', function() {
		// Insertion mobile wizard toolbar button is disabled by default
		cy.get('#tb_actionbar_item_undo')
			.should('have.class', 'disabled')

		// Click on edit button
		cy.get('#mobile-edit-button').click()

		// Button should be still disabled
		cy.get('#tb_actionbar_item_undo')
			.should('have.class', 'disabled')

		// Type somthing in the document
		cy.get('#document-container').type('x')

		// Button should become enabled
		cy.get('#tb_actionbar_item_undo')
			.should('not.have.class', 'disabled')
	})

	it('State of redo toolbar item.', function() {
		// Insertion mobile wizard toolbar button is disabled by default
		cy.get('#tb_actionbar_item_redo')
			.should('have.class', 'disabled')

		// Click on edit button
		cy.get('#mobile-edit-button').click()

		// Button should be still disabled
		cy.get('#tb_actionbar_item_redo')
			.should('have.class', 'disabled')

		// Type somthing in the document
		cy.get('#document-container').type('x')

		// Button should be still disabled
		cy.get('#tb_actionbar_item_redo')
			.should('have.class', 'disabled')

		// Do an undo
		cy.get('#tb_actionbar_item_undo')
			.should('not.have.class', 'disabled')
		cy.get('#tb_actionbar_item_undo').click()

		// Button should become enabled
		cy.get('#tb_actionbar_item_redo')
			.should('not.have.class', 'disabled')
	})

	it('Open and close mobile wizard by toolbar item.', function() {
		// Click on edit button
		cy.get('#mobile-edit-button').click()

		// Click on mobile wizard toolbar item
		cy.get('#tb_actionbar_item_mobile_wizard')
			.should('not.have.class', 'disabled')
			.click()

		// Mobile wizard is opened and it has any content
		cy.get('#mobile-wizard-content')
			.should('not.be.empty');

		// Toolbar button is checked
		cy.get('#tb_actionbar_item_mobile_wizard table')
			.should('have.class', 'checked')

		cy.get('#tb_actionbar_item_mobile_wizard')
			.click()

		// Mobile wizard is closed
		cy.get('#mobile-wizard').should('not.be.visible');

		cy.get('#tb_actionbar_item_mobile_wizard table')
			.should('not.have.class', 'checked')
	})

	it('Open and close insertion mobile wizard by toolbar item.', function() {
		// Click on edit button
		cy.get('#mobile-edit-button').click()

		// Click on toolbar item
		cy.get('#tb_actionbar_item_insertion_mobile_wizard')
			.should('not.have.class', 'disabled')
			.click()

		// Mobile wizard is opened and it has any content
		cy.get('#mobile-wizard-content')
			.should('not.be.empty');

		// Toolbar button is checked
		cy.get('#tb_actionbar_item_insertion_mobile_wizard table')
			.should('have.class', 'checked')

		// Click on toolbar item again
		cy.get('#tb_actionbar_item_insertion_mobile_wizard')
			.click()

		// Mobile wizard is closed
		cy.get('#mobile-wizard').should('not.be.visible');

		cy.get('#tb_actionbar_item_insertion_mobile_wizard table')
			.should('not.have.class', 'checked')

		// Open mobile wizard again
		cy.get('#tb_actionbar_item_insertion_mobile_wizard')
			.click()

		// Mobile wizard is opened and it has any content
		cy.get('#mobile-wizard-content')
			.should('not.be.empty');
	})

	it('Open insert comment dialog by toolbar item.', function() {
		// Click on edit button
		cy.get('#mobile-edit-button').click()

		// Click on toolbar item
		cy.get('#tb_actionbar_item_insertcomment')
			.should('not.have.class', 'disabled')
			.click()

		// Comment insertion dialog is opened
		cy.get('.loleaflet-annotation-table')
			.should('be.visible');

		// Close the dialog
		cy.contains('Cancel')
			.click()

		cy.get('.loleaflet-annotation-table')
			.should('be.not.visible');
	})
})
