/* global cy Cypress expect require */

require('cypress-wait-until');

function loadTestDoc(fileName, subFolder, mobile) {
	cy.log('Loading test document - start.');
	cy.log('Param - fileName: ' + fileName);
	cy.log('Param - subFolder: ' + subFolder);
	cy.log('Param - mobile: ' + mobile);

	// Get a clean test document
	if (subFolder === undefined) {
		cy.task('copyFile', {
			sourceDir: Cypress.env('DATA_FOLDER'),
			destDir: Cypress.env('WORKDIR'),
			fileName: fileName,
		});
	} else {
		cy.task('copyFile', {
			sourceDir: Cypress.env('DATA_FOLDER') + subFolder + '/',
			destDir: Cypress.env('WORKDIR') + subFolder + '/',
			fileName: fileName,
		});
	}

	if (mobile === true) {
		cy.viewport('iphone-6');
	}

	// Open test document
	var URI;
	if (subFolder === undefined) {
		URI = 'http://localhost:'+
			Cypress.env('SERVER_PORT') +
			'/loleaflet/' +
			Cypress.env('WSD_VERSION_HASH') +
			'/loleaflet.html?lang=en-US&file_path=file://' +
			Cypress.env('WORKDIR') + fileName;
	} else {
		URI = 'http://localhost:'+
			Cypress.env('SERVER_PORT') +
			'/loleaflet/' +
			Cypress.env('WSD_VERSION_HASH') +
			'/loleaflet.html?lang=en-US&file_path=file://' +
			Cypress.env('WORKDIR') + subFolder + '/' + fileName;
	}

	cy.log('Loading: ' + URI);
	cy.visit(URI, {
		onLoad: function(win) {
			win.onerror = cy.onUncaughtException;
		}});

	// Wait for the document to fully load
	cy.get('.leaflet-tile-loaded', {timeout : 10000});

	cy.log('Loading test document - end.');
}

// Assert that NO keyboard input is accepted (i.e. keyboard should be HIDDEN).
function assertNoKeyboardInput() {
	cy.get('textarea.clipboard')
		.should('have.attr', 'data-accept-input', 'false');
}

// Assert that keyboard input is accepted (i.e. keyboard should be VISIBLE).
function assertHaveKeyboardInput() {
	cy.get('textarea.clipboard')
		.should('have.attr', 'data-accept-input', 'true');
}

// Assert that we have cursor and focus.
function assertCursorAndFocus() {
	cy.log('Verifying Cursor and Focus.');

	// Active element must be the textarea named clipboard.
	cy.document().its('activeElement.className')
		.should('be.eq', 'clipboard');

	// In edit mode, we should have the blinking cursor.
	cy.get('.leaflet-cursor.blinking-cursor')
		.should('exist');
	cy.get('.leaflet-cursor-container')
		.should('exist');

	assertHaveKeyboardInput();

	cy.log('Cursor and Focus verified.');
}

// Select all text via CTRL+A shortcut.
function selectAllText(assertFocus = true) {
	if (assertFocus)
		assertCursorAndFocus();

	cy.log('Select all text');

	// Trigger select all
	cy.get('textarea.clipboard')
		.type('{ctrl}a');

	cy.get('.leaflet-marker-icon')
		.should('exist');
}

// Clear all text by selecting all and deleting.
function clearAllText() {
	assertCursorAndFocus();

	cy.log('Clear all text');

	// Trigger select all
	cy.get('textarea.clipboard')
		.type('{ctrl}a');

	cy.get('.leaflet-marker-icon')
		.should('exist');

	// Then remove
	cy.get('textarea.clipboard')
		.type('{del}');

	cy.get('.leaflet-marker-icon')
		.should('not.exist');
}

// Check that the clipboard text matches with the specified text.
function expectTextForClipboard(expectedPlainText) {
	doIfInWriter(function() {
		cy.get('#copy-paste-container p font')
			.should('have.text', expectedPlainText);
	});
	doIfInCalc(function() {
		cy.get('#copy-paste-container pre')
			.should('have.text', expectedPlainText);
	});
	doIfInImpress(function() {
		cy.get('#copy-paste-container pre')
			.should('have.text', expectedPlainText);
	});
}

// Check that the clipboard text matches with the
// passed regular expression.
function matchClipboardText(regexp) {
	doIfInWriter(function() {
		cy.contains('#copy-paste-container p font', regexp)
			.should('exist');
	});
	doIfInCalc(function() {
		cy.contains('#copy-paste-container pre', regexp)
			.should('exist');
	});
	doIfInImpress(function() {
		cy.contains('#copy-paste-container pre', regexp)
			.should('exist');
	});
}

function beforeAllDesktop(fileName, subFolder) {
	var mobile = false;
	loadTestDoc(fileName, subFolder, mobile);
}

function afterAll(fileName) {
	cy.log('Waiting for closing the document - start.');
	cy.log('Param - fileName: ' + fileName);

	// Make sure that the document is closed
	cy.visit('http://admin:admin@localhost:' +
			Cypress.env('SERVER_PORT') +
			'/loleaflet/dist/admin/admin.html');

	cy.get('#uptime')
		.should('not.have.text', '0');

	// We have all lines of document infos as one long string.
	// We have PID number before the file names, with matching
	// also on the PID number we can make sure to match on the
	// whole file name, not on a suffix of a file name.
	var regex = new RegExp('[0-9]' + fileName);
	cy.get('#docview')
		.invoke('text')
		.should('not.match', regex);

	cy.log('Waiting for closing the document - end.');
}


function initAliasToNegative(aliasName) {
	cy.log('Initializing alias to a negative value - start.');
	cy.log('Param - aliasName: ' + aliasName);

	cy.get('#copy-paste-container')
		.invoke('offset')
		.its('top')
		.as(aliasName);

	cy.get('@' + aliasName)
		.should('be.lessThan', 0);

	cy.log('Initializing alias to a negative value - end.');
}

function initAliasToEmptyString(aliasName) {
	cy.log('Initializing alias to empty string - start.');
	cy.log('Param - aliasName: ' + aliasName);

	// Do an empty slice to generate empty string
	cy.get('#copy-paste-container')
		.invoke('css', 'display')
		.invoke('slice', '0', '0')
		.as(aliasName);

	cy.get('@' + aliasName)
		.should('be.equal', '');

	cy.log('Initializing alias to empty string - end.');
}

function doIfInCalc(callback) {
	cy.get('#document-container')
		.then(function(doc) {
			if (doc.hasClass('spreadsheet-doctype')) {
				callback();
			}
		});
}

function doIfInImpress(callback) {
	cy.get('#document-container')
		.then(function(doc) {
			if (doc.hasClass('presentation-doctype')) {
				callback();
			}
		});
}

function doIfInWriter(callback) {
	cy.get('#document-container')
		.then(function(doc) {
			if (doc.hasClass('text-doctype')) {
				callback();
			}
		});
}

// Types text into elem with a delay in between characters.
// Sometimes cy.type results in random character insertion,
// this avoids that, which is not clear why it happens.
function typeText(selector, text, delayMs=0) {
	var elem= cy.get(selector);
	for (var i = 0; i < text.length; i++) {
		elem.type(text.charAt(i));
		if (delayMs > 0)
			cy.wait(delayMs);
	}
}

function getLOVersion() {
	var versionString = Cypress.env('LO_CORE_VERSION');
	if (versionString.includes('Collabora')) {
		if (versionString.includes(' 6.2.')) {
			return 'cp-6-2';
		} else if (versionString.includes(' 6.4.')) {
			return 'cp-6-4';
		}
	}
	return 'master';
}

function imageShouldBeFullWhiteOrNot(selector, fullWhite = true) {
	cy.get(selector)
		.should(function(images) {
			var img = images[0];

			// Create an offscreen canvas to check the image's pixels
			var canvas = document.createElement('canvas');
			canvas.width = img.width;
			canvas.height = img.height;
			canvas.getContext('2d').drawImage(img, 0, 0, img.width, img.height);
			var context = canvas.getContext('2d');

			// Ignore a small zone on the edges, to ignore border.
			var ignoredPixels = 2;
			var pixelData = context.getImageData(ignoredPixels, ignoredPixels,
				img.width - 2 * ignoredPixels,
				img.height - 2 * ignoredPixels).data;

			var allIsWhite = true;
			for (var i = 0; i < pixelData.length; ++i) {
				allIsWhite = allIsWhite && pixelData[i] == 255;
			}
			if (fullWhite)
				expect(allIsWhite).to.be.true;
			else
				expect(allIsWhite).to.be.false;
		});
}

function waitUntilIdle(selector, content) {
	cy.log('Waiting item to be idle - start.');

	var item;
	var waitingTime = 2000;
	if (content) {
		cy.contains(selector, content, { log: false })
			.then(function(itemToIdle) {
				item = itemToIdle;
			});

		cy.waitUntil(function() {
			cy.wait(waitingTime);

			return cy.contains(selector, content, { log: false })
				.then(function(itemToIdle) {
					if (Cypress.dom.isDetached(item[0])) {
						cy.log('Item is detached.');
						item = itemToIdle;
						return false;
					} else {
						return true;
					}
				});
		});
	} else {
		cy.get(selector, { log: false })
			.then(function(itemToIdle) {
				item = itemToIdle;
			});

		cy.waitUntil(function() {
			cy.wait(waitingTime);

			return cy.get(selector, { log: false })
				.then(function(itemToIdle) {
					if (Cypress.dom.isDetached(item[0])) {
						cy.log('Item is detached.');
						item = itemToIdle;
						return false;
					} else {
						return true;
					}
				});
		});
	}

	cy.log('Waiting item to be idle - end.');
}

// This is a workaround for avoid 'item detached from DOM'
// failures caused by GUI flickering.
// GUI flickering might mean bad design, but
// until it's fixed we can use this method.
// Known GUI flickering:
// * mobile wizard
// IMPORTANT: don't use this if there is no
// flickering. Use simple click() instead. This method
// is much slower.
function clickOnIdle(selector, content) {
	cy.log('Clicking on item when idle - start.');

	waitUntilIdle(selector, content);

	if (content) {
		cy.contains(selector, content)
			.then(function(item) {
				expect(item).to.have.length(1);
				if (Cypress.dom.isHidden(item[0])) {
					cy.contains(selector, content)
						.scrollIntoView();

					waitUntilIdle(selector, content);

					cy.contains(selector, content)
						.should('be.visible');
				}
			});
		cy.contains(selector, content)
			.click();
	} else {
		cy.get(selector)
			.then(function(item) {
				expect(item).to.have.length(1);
				if (Cypress.dom.isHidden(item[0])) {
					cy.get(selector)
						.scrollIntoView();

					waitUntilIdle(selector);

					cy.get(selector)
						.should('be.visible');
				}
			});
		cy.get(selector)
			.click();
	}

	cy.log('Clicking on item when idle - end.');
}

// See comments at clickOnIdle() method.
function inputOnIdle(selector, input) {
	cy.log('Type into an input item when idle - start.');

	waitUntilIdle(selector);

	cy.get(selector)
		.clear()
		.type(input)
		.type('{enter}');

	cy.log('Type into an input item when idle - end.');
}

module.exports.loadTestDoc = loadTestDoc;
module.exports.assertCursorAndFocus = assertCursorAndFocus;
module.exports.assertNoKeyboardInput = assertNoKeyboardInput;
module.exports.assertHaveKeyboardInput = assertHaveKeyboardInput;
module.exports.selectAllText = selectAllText;
module.exports.clearAllText = clearAllText;
module.exports.expectTextForClipboard = expectTextForClipboard;
module.exports.matchClipboardText = matchClipboardText;
module.exports.afterAll = afterAll;
module.exports.initAliasToNegative = initAliasToNegative;
module.exports.initAliasToEmptyString = initAliasToEmptyString;
module.exports.doIfInCalc = doIfInCalc;
module.exports.doIfInImpress = doIfInImpress;
module.exports.doIfInWriter = doIfInWriter;
module.exports.beforeAllDesktop = beforeAllDesktop;
module.exports.typeText = typeText;
module.exports.getLOVersion = getLOVersion;
module.exports.imageShouldBeFullWhiteOrNot = imageShouldBeFullWhiteOrNot;
module.exports.clickOnIdle = clickOnIdle;
module.exports.inputOnIdle = inputOnIdle;
