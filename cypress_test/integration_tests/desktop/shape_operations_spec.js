/* global describe it cy require expect afterEach*/

var helper = require('../common/helper');

describe('Shape operations', function() {

	afterEach(function() {
		helper.afterAll();
	});

	it('Insert a simple shape.', function() {
		helper.loadTestDoc('empty.odt');

		// Scroll on the up toolbar
		cy.get('#toolbar-up .w2ui-scroll-right').click();
		cy.get('.w2ui-tb-image.w2ui-icon.basicshapes_ellipse')
			.should('be.visible')
			.click();

		// Insert a rectangle
		cy.get('.col.w2ui-icon.basicshapes_rectangle')
			.click({force : true});

		cy.get('.leaflet-pane.leaflet-overlay-pane svg g')
			.should('exist');

		// Check whether the rectangle was inserted as an SVG
		cy.get('.leaflet-pane.leaflet-overlay-pane svg')
			.should('have.class', 'leaflet-zoom-animated')
			.then(function(svg) {
				expect(svg.length).to.be.greaterThan(0);
				expect(svg[0].getBBox().width).to.be.greaterThan(0);
				expect(svg[0].getBBox().height).to.be.greaterThan(0);
			});
	});
});
