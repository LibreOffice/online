/* global cy Cypress*/

function removeTextSelection() {
	cy.log('Removing text selection - start.');

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
			.invoke('attr', 'style')
			.should('contain', '-8px,');
	}

	cy.log('Removing text selection - end.');
}

function selectAllMobile(removeSelection = true) {
	cy.log('Selecting all text - start.');

	if (removeSelection)
		removeTextSelection();

	cy.get('#spreadsheet-header-corner')
		.click();

	cy.get('.spreadsheet-cell-resize-marker:nth-of-type(2)')
		.should('not.be.visible');

	cy.get('.spreadsheet-cell-resize-marker:nth-of-type(1)')
		.invoke('attr', 'style')
		.should('contain', '(-9px, -8px,');

	cy.log('Selecting all text - end.');
}

module.exports.removeTextSelection = removeTextSelection;
module.exports.selectAllMobile = selectAllMobile;
