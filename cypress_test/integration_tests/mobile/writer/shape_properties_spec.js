/* global describe it cy beforeEach require afterEach Cypress*/

var helper = require('../../common/helper');

describe('Change shape properties via mobile wizard.', function() {
	beforeEach(function() {
		helper.beforeAllMobile('empty.odt', 'writer');

		// Click on edit button
		cy.get('#mobile-edit-button').click();

		// Open insertion wizard
		cy.get('#tb_actionbar_item_insertion_mobile_wizard')
			.click();

		// Do insertion
		cy.get('.menu-entry-with-icon')
			.contains('Shape')
			.click();

		cy.get('.basicshapes_right-triangle').
			click();

		// Check that the shape is there
		cy.get('.leaflet-pane.leaflet-overlay-pane svg g svg g')
			.should('have.class', 'com.sun.star.drawing.CustomShape');
	});

	afterEach(function() {
		helper.afterAll();
	});

	function triggerNewSVG() {
		helper.clearMobileWizardState();

		cy.get('#tb_actionbar_item_mobile_wizard')
			.click();
		cy.get('#mobile-wizard')
			.should('be.visible');

		// Change width
		cy.get('#PosSizePropertyPanel')
			.click();

		cy.get('#selectwidth .plus')
			.should('be.visible')
			.click();
	}

	function openPosSizePanel() {
		// Open mobile wizard
		cy.get('#tb_actionbar_item_mobile_wizard')
			.click();

		cy.get('#PosSizePropertyPanel')
			.click();

		cy.get('.ui-content.level-0.mobile-wizard')
			.should('be.visible')
			.wait(100);
	}

	function openLinePropertyPanel() {
		// Open mobile wizard
		cy.get('#tb_actionbar_item_mobile_wizard')
			.click();

		cy.get('#mobile-wizard')
			.should('be.visible');

		cy.get('#LinePropertyPanel')
			.click();

		cy.get('.ui-content.level-0.mobile-wizard')
			.should('be.visible')
			.wait(100);
	}

	it('Check default shape geometry.', function() {
		// Geometry
		cy.get('.leaflet-pane.leaflet-overlay-pane svg g svg g g g path')
			.should('have.attr', 'd', 'M 1965,4810 L 7957,10802 1965,10802 1965,4810 1965,4810 Z');
		// Fill color
		cy.get('.leaflet-pane.leaflet-overlay-pane svg g svg g g g path')
			.should('have.attr', 'fill', 'rgb(114,159,207)');
	});

	it('Change shape width.', function() {
		// TODO: Entering a value inside the spinbutton has no effect on the shape.
		if (Cypress.env('LO_CORE_VERSION') === 'master')
			return;

		openPosSizePanel();

		cy.get('#selectwidth .spinfield')
			.clear()
			.type('4.2')
			.type('{enter}');

		cy.get('.leaflet-pane.leaflet-overlay-pane svg g svg g g g path')
			.should('have.attr', 'd', 'M 1965,4810 L 12635,10802 1965,10802 1965,4810 1965,4810 Z');
	});

	it('Change shape height.', function() {
		// TODO: Entering a value inside the spinbutton has no effect on the shape.
		if (Cypress.env('LO_CORE_VERSION') === 'master')
			return;

		openPosSizePanel();

		cy.get('#selectheight .spinfield')
			.clear()
			.type('5.2')
			.type('{enter}');

		cy.get('.leaflet-pane.leaflet-overlay-pane svg g svg g g g path')
			.should('have.attr', 'd', 'M 1965,4810 L 7957,18020 1965,18020 1965,4810 1965,4810 Z');
	});

	it('Change size with keep ratio enabled.', function() {
		// TODO: Entering a value inside the spinbutton has no effect on the shape.
		if (Cypress.env('LO_CORE_VERSION') === 'master')
			return;

		openPosSizePanel();

		// Enable keep ratio
		cy.get('#ratio #ratio')
			.click();

		cy.get('#ratio #ratio')
			.should('have.attr', 'checked', 'checked');

		// Change height
		cy.get('#selectheight .spinfield')
			.clear()
			.type('5.2')
			.type('{enter}');

		cy.get('.leaflet-pane.leaflet-overlay-pane svg g svg g g g path')
			.should('have.attr', 'd', 'M 1965,4810 L 15175,18020 1965,18020 1965,4810 1965,4810 Z');
	});

	it('Vertical mirroring', function() {
		openPosSizePanel();

		cy.get('#FlipVertical')
			.click();

		cy.get('.leaflet-pane.leaflet-overlay-pane svg g svg g g g path')
			.should('have.attr', 'd', 'M 1965,10800 L 7957,4808 1965,4808 1965,10800 1965,10800 Z');
	});

	it('Horizontal mirroring', function() {
		openPosSizePanel();

		cy.get('#FlipHorizontal')
			.click();

		cy.get('.leaflet-pane.leaflet-overlay-pane svg g svg g g g path')
			.should('have.attr', 'd', 'M 1965,4810 L 7957,10802 1965,10802 1965,4810 1965,4810 Z');
	});

	it('Trigger moving backward / forward', function() {
		openPosSizePanel();

		// We can't test the result, so we just trigger
		// the events to catch crashes, consoler errors.
		cy.get('#BringToFront')
			.click();
		cy.wait(300);

		cy.get('#ObjectForwardOne')
			.click();
		cy.wait(300);

		cy.get('#ObjectBackOne')
			.click();
		cy.wait(300);

		cy.get('#SendToBack')
			.click();
	});

	it('Change line color', function() {
		// TODO: Layout of the line properties panel is completely broken.
		if (Cypress.env('LO_CORE_VERSION') === 'master')
			return;

		openLinePropertyPanel();

		cy.get('#XLineColor')
			.click();

		cy.get('.ui-content[title="Line Color"] .color-sample-small[style="background-color: rgb(152, 0, 0);"]')
			.click();

		triggerNewSVG();

		cy.get('.leaflet-pane.leaflet-overlay-pane svg g svg g g g path[fill="none"]')
			.should('have.attr', 'stroke', 'rgb(152,0,0)');
	});

	it('Change line style', function() {
		// TODO: Layout of the line properties panel is completely broken.
		if (Cypress.env('LO_CORE_VERSION') === 'master')
			return;

		openLinePropertyPanel();

		cy.get('#linestyle')
			.click();

		cy.get('.ui-combobox-text')
			.contains('Dashed')
			.click();

		triggerNewSVG();

		cy.get('.leaflet-pane.leaflet-overlay-pane svg g svg g g g path[fill="none"]')
			.should('have.length.greaterThan', 12);
	});

	it('Change line width', function() {
		// TODO: Layout of the line properties panel is completely broken.
		if (Cypress.env('LO_CORE_VERSION') === 'master')
			return;

		openLinePropertyPanel();

		cy.get('#linewidth .spinfield')
			.should('have.attr', 'readonly', 'readonly');

		cy.get('#linewidth .plus')
			.click();

		triggerNewSVG();

		cy.get('.leaflet-pane.leaflet-overlay-pane svg g svg g g g path[fill="none"]')
			.should('have.attr', 'stroke-width', '141');

		helper.clearMobileWizardState();

		openLinePropertyPanel();

		cy.get('#linewidth .minus')
			.click();

		triggerNewSVG();

		cy.get('.leaflet-pane.leaflet-overlay-pane svg g svg g g g path[fill="none"]')
			.should('have.attr', 'stroke-width', '141');
	});

	it('Change line transparency', function() {
		// TODO: Layout of the line properties panel is completely broken.
		if (Cypress.env('LO_CORE_VERSION') === 'master')
			return;

		openLinePropertyPanel();

		cy.get('#linetransparency .spinfield')
			.clear()
			.type('20')
			.type('{enter}');

		triggerNewSVG();

		cy.get('.leaflet-pane.leaflet-overlay-pane svg g svg g g g defs mask linearGradient')
			.should('exist');
	});

	it('Arrow style items are hidden.', function() {
		// TODO: Layout of the line properties panel is completely broken.
		if (Cypress.env('LO_CORE_VERSION') === 'master')
			return;

		openLinePropertyPanel();

		cy.get('#linestyle')
			.should('be.visible');

		cy.get('#beginarrowstyle')
			.should('not.exist');

		cy.get('#endarrowstyle')
			.should('not.exist');

	});
});
