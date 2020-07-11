/* global describe it cy beforeEach require afterEach expect*/

var helper = require('../../common/helper');
var mobileHelper = require('../../common/mobile_helper');
var writerMobileHelper = require('./writer_mobile_helper');

describe('Don\'t show caret when text is selected.', function() {
	var testFileName = 'apply_font.odt';

	beforeEach(function() {
		mobileHelper.beforeAllMobile(testFileName, 'writer');

		// Click on edit button
		mobileHelper.enableEditingMobile();

		// Do a new selection
		writerMobileHelper.selectAllMobile();
	});

	afterEach(function() {
		helper.afterAll(testFileName);
	});

	it('Control input caret\'s existince (it souldn\'t)', function() {
		//cy.get('leaflet-marker-icon leaflet-cursor-handler leaflet-zoom-animated leaflet-interactive leaflet-marker-draggable').should('not.exist');
		writerMobileHelper.selectAllMobile();

		// Get end marker, get its position, get the position where caret marker should be.
		cy.get('.leaflet-selection-marker-end').then(function(markers) {
			expect(markers.length).to.be.equal(1);
			var XPos = markers[0].getBoundingClientRect().right + 3;
			var YPos = markers[0].getBoundingClientRect().top + 2;

			// Remove selection
			mobileHelper.tapOnDocument(XPos, YPos);
			cy.wait(500); // Prevent double click.
			mobileHelper.tapOnDocument(XPos, YPos); // Now the caret marker should appear.

			mobileHelper.tapOnDocument(XPos, YPos); // Now we want a double click.
			mobileHelper.tapOnDocument(XPos, YPos);
			cy.wait(500); // Wait for the marker to appear.
			cy.get('div[class=".leaflet-marker-icon .leaflet-cursor-handler .leaflet-zoom-animated .leaflet-interactive .leaflet-marker-draggable"]').should('have.length', 0);
			//cy.get('div[class=".leaflet-cursor-handler"]').should('have.length', 0);
		});
	});
});