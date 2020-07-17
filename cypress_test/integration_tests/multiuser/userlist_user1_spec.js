/* global describe it cy beforeEach require afterEach */

var helper = require('../common/helper');

describe('Check user list with user-1.', function() {
	var testFileName = 'userlist.odt';

	beforeEach(function() {
		helper.beforeAllDesktop(testFileName);
	});

	afterEach(function() {
		helper.afterAll(testFileName);
	});

	it('Userlist visibility.', function() {
		// Here user-2 will load the same document
		cy.get('body')
			.click();

		cy.get('#tb_actionbar_item_userlist', {timeout : 30000})
			.should('be.visible', {timeout : 30000});

		cy.get('#tb_actionbar_item_userlist .w2ui-tb-caption')
			.should('have.text', '2 users');

		// Leave a comment for user2, that we finished
		cy.get('#menu-insert')
			.click();

		cy.get('#menu-insertcomment')
			.click();

		cy.get('.loleaflet-annotation-edit:has(#annotation-save) .loleaflet-annotation-textarea')
			.type('Done');

		cy.get('#annotation-save')
			.click();

		// Here user-2 will exit
		// This is a bug, that userlist is not updated
		//cy.get('#tb_actionbar_item_userlist')
		//	.should('not.be.visible', {timeout : 30000});
	});

});
