/* global describe it cy beforeEach require afterEach */

var helper = require('../common/helper');

describe('Check user list with user-2.', function() {
	var testFileName = 'userlist.odt';

	beforeEach(function() {
		// Wait here, before loading the document.
		// Opening two clients at the same time causes an issue.
		cy.wait(5000);
		helper.beforeAllDesktop(testFileName);
	});

	afterEach(function() {
		helper.afterAll(testFileName);
	});

	it('Userlist visibility.', function() {
		// user-1 already loaded the document
		cy.get('body')
			.click();

		cy.get('#tb_actionbar_item_userlist', {timeout : 30000})
			.should('be.visible', {timeout : 30000});

		cy.get('#tb_actionbar_item_userlist .w2ui-tb-caption')
			.should('have.text', '2 users');

		// user-1 adds a comment, so we can close this view
		cy.get('.loleaflet-annotation-content-wrapper')
			.should('exist', {timeout : 30000});
	});
});
