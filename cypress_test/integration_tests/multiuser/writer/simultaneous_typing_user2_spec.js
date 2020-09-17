/* global describe it cy beforeEach require afterEach */

var helper = require('../../common/helper');

describe('Simultaneous typing: user-2.', {retries : 0}, function() {
	var testFileName = 'simultaneous_typing.odt';

	beforeEach(function() {
		// Wait here, before loading the document.
		// Opening two clients at the same time causes an issue.
		cy.wait(5000);
		helper.beforeAll(testFileName, 'writer', true);
	});

	afterEach(function() {
		helper.afterAll(testFileName);
	});

	it('Simultaneous typing.', function() {
		// user-1 loads the same document

		cy.get('#tb_actionbar_item_userlist')
			.should('be.visible');

		cy.get('#tb_actionbar_item_userlist .w2ui-tb-caption')
			.should('have.text', '2 users');

		// Now type some text, while user-1 does the same.
		var text = 'xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx';
		helper.typeText('textarea.clipboard', text, 100);

		helper.selectAllText();

		cy.get('#copy-paste-container p')
			.should('contain.text', text);
	});
});
