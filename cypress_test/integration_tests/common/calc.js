/* global cy expect */

// Click on the formula bar.
// mouseover is triggered to avoid leaving the mouse on the Formula-Bar,
// which shows the tooltip and messes up tests.
function clickFormulaBar() {

	// The inputbar_container is 100% width, which
	// can extend behind the sidebar. So we can't
	// rely on its width. Instead, we rely on the
	// canvas, which is accurately sized.
	// N.B. Setting the width of the inputbar_container
	// is futile because it messes the size of the canvas.
	cy.get('.inputbar_canvas')
		.then(function(items) {
			expect(items).to.have.lengthOf(1);
			var XPos = items[0].getBoundingClientRect().width / 2;
			var YPos = items[0].getBoundingClientRect().height / 2;
			cy.get('.inputbar_container')
				.click(XPos, YPos);
		});

	cy.get('body').trigger('mouseover');
}

function clickOnFirstCell(firstClick = true, dblClick = false) {
	cy.log('Clicking on first cell - start.');
	cy.log('Param - firstClick: ' + firstClick);
	cy.log('Param - dblClick: ' + dblClick);

	// Use the tile's edge to find the first cell's position
	cy.get('#map')
		.then(function(items) {
			expect(items).to.have.lengthOf(1);
			var XPos = items[0].getBoundingClientRect().left + 10;
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
			.should('be.visible');
	else
		cy.get('.leaflet-cursor.blinking-cursor')
			.should('be.visible');

	cy.log('Clicking on first cell - end.');
}

function dblClickOnFirstCell() {
	clickOnFirstCell(false, true);
}

module.exports.clickOnFirstCell = clickOnFirstCell;
module.exports.dblClickOnFirstCell = dblClickOnFirstCell;
module.exports.clickFormulaBar = clickFormulaBar;
