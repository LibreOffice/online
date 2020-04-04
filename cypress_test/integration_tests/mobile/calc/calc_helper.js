/* global cy expect require Cypress*/

var helper = require('../../common/helper');

function clickOnFirstCell(firstClick = true, dblClick = false) {
	// Enable editing if it's in read-only mode
	helper.enableEditingMobile();

	// TODO: it seems '.leaflet-tile-container' gets into an
	// invalid state where it's position is negative.
	cy.waitUntil(function() {
		return cy.get('.leaflet-tile-container')
			.then(function(items) {
				expect(items).to.have.lengthOf(1);
				return items[0].getBoundingClientRect().top >= 0 && items[0].getBoundingClientRect().right >= 0;
			});
	});

	// Use the tile's edge to find the first cell's position
	cy.get('.leaflet-tile-container')
		.then(function(items) {
			expect(items).to.have.lengthOf(1);
			var XPos = items[0].getBoundingClientRect().right + 10;
			var YPos = items[0].getBoundingClientRect().top + 10;
			if (dblClick) {
                cy.get('body')
                    .dblclick(XPos, YPos);
            } else {
                cy.get('body')
                    .click(XPos, YPos);
            }
		});

	if (firstClick && !dblClick)
		cy.get('.spreadsheet-cell-resize-marker')
			.should('exist');
	else
		cy.get('.leaflet-cursor.blinking-cursor')
			.should('exist');
}

function dblClickOnFirstCell() {
	clickOnFirstCell(dblClick = true);
}

function copyContentToClipboard() {
	selectAllMobile();

	cy.get('.leaflet-tile-container')
		.then(function(items) {
			expect(items).to.have.lengthOf(1);
			var XPos = items[0].getBoundingClientRect().right + 10;
			var YPos = items[0].getBoundingClientRect().top + 10;
			helper.longPressOnDocument(XPos, YPos);
		});

	cy.get('#mobile-wizard')
		.should('be.visible');

	// Execute copy
	cy.get('.menu-entry-with-icon', {timeout : 10000})
		.contains('Copy')
		.click();

	// Close warning about clipboard operations
	cy.get('.vex-dialog-button-primary.vex-dialog-button.vex-first')
		.click();

	// Wait until it's closed
	cy.get('.vex-overlay')
		.should('not.exist');
}

function removeTextSelection() {
	// TODO: select all does not work with core/master
	// if we have a column selected
	if (Cypress.env('LO_CORE_VERSION') === 'master') {
		cy.get('body')
			.type('{enter}');

		cy.get('.leaflet-marker-icon')
			.should('exist');
	} else {
		cy.get('.spreadsheet-header-columns')
			.click();

		cy.get('.spreadsheet-cell-resize-marker')
			.should('exist');
	}
}

function selectAllMobile() {
	removeTextSelection();


	cy.get('#spreadsheet-header-corner')
		.click();

	cy.get('.leaflet-marker-icon')
		.should('exist');
}

module.exports.copyContentToClipboard = copyContentToClipboard;
module.exports.removeTextSelection = removeTextSelection;
module.exports.selectAllMobile = selectAllMobile;
module.exports.clickOnFirstCell = clickOnFirstCell;
module.exports.dblClickOnFirstCell = dblClickOnFirstCell;
