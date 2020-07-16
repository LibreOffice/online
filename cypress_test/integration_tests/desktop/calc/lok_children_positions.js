/* global describe it cy beforeEach require afterEach expect */

/*
    A little explanation:
        We will simulate a click on sidebar. Sidebar contains an image sent from core.
        Since it is an image and does not contain HTML elements, we will click on a coordinate.
        If in the future, the sidebar's element positioning changes on the core side, this test will have to be upadated too.
        And of course id based "get" statements rely on fixed names.
*/

var helper = require('../../common/helper');
var calcHelper = require('../../common/calc_helper');

function fontSelectionChild()
{
	it('Check sidebar font selection child.', function() {
		calcHelper.clickOnFirstCell(); // Give map the focus.
		var sideBarX;
		var sideBarY;
		cy.get('#' + calcHelper.getSideBarID()).then(function(items) { // Get the sidebar.
			var clickX = 326;
			var clickY = 112;
			sideBarX = items[0].getBoundingClientRect().left; // Get coordinates.
			sideBarY = items[0].getBoundingClientRect().top;
			calcHelper.clickOnSidebar(clickX, clickY); // We know the coordinates of the dropdown button. If core UI changes, this will change.
			cy.wait(500); // Wait for the focus.
			calcHelper.clickOnSidebar(clickX, clickY);
			cy.wait(500); // Wait for the child window to open.

			var childX;
			var childY;
			cy.get('[id*="floating"]').then(function(items) { // At the time of testing, only one lok window with id like 'floating' will be present.
				expect(items).to.have.lengthOf(1); // We should have our child window.
				childX = items[0].getBoundingClientRect().left + items[0].getBoundingClientRect().width;
				childY = items[0].getBoundingClientRect().top;

				var diffX = Math.abs(sideBarX + clickX - childX); // Calculate differences between child window and
				var diffY = Math.abs(sideBarY + clickY - childY);
				expect(diffX).to.be.lessThan(120, 'Sidebar dropdown X offset is too much.');
				expect(diffY).to.be.lessThan(40, 'Sidebar dropdown Y offset is too much.');
			});
		});
	});
}

describe('Lok child dialog offset test.', function() {
	var testFileName = 'focus.ods';

	beforeEach(function() {
		helper.beforeAllDesktop(testFileName, 'calc');

		// Wait until the Formula-Bar is loaded.
		cy.get('.inputbar_container', {timeout : 10000});
	});

	afterEach(function() {
		helper.afterAll(testFileName);
	});

	fontSelectionChild();
});