/* global require */

var process = require('process');
var tasks = require('./tasks');
var selectTests = require('cypress-select-tests');

function plugin(on, config) {
	on('task', {
		copyFile: tasks.copyFile,
		failed: require('cypress-failed-log/src/failed')()
	});

	on('before:browser:launch', function(browser, launchOptions) {
		if (browser.family === 'chromium' && process.env.ENABLE_LOGGING) {
			launchOptions.args.push('--enable-logging=stderr');
			launchOptions.args.push('--v=2');
			return launchOptions;
		}
	});

	on('file:preprocessor', selectTests(config, pickTests));

	return config;
}

var testBlackLists = {
	'master': [
		['mobile/writer/table_properties_spec.js',
			[]
		],
		['mobile/writer/shape_properties_spec.js',
			[
				'Change size with keep ratio enabled.',
				'Change line color',
				'Change line style',
				'Change line width',
				'Change line transparency',
				'Arrow style items are hidden.'
			]
		],
		['mobile/writer/apply_paragraph_properties_spec.js',
			[
				'Apply default bulleting.',
				'Apply default numbering.',
				'Apply background color.'
			]
		],
		['mobile/writer/insert_object_spec.js',
			[
				'Insert default table.',
				'Insert custom table.'
			]
		],
		['mobile/writer/apply_font_spec.js',
			[
				'Insert default table.',
				'Insert custom table.'
			]
		],
		['mobile/calc/number_format_spec.js',
			[
				'Select percent format from list.',
				'Push percent button.',
				'Select currency format from list.',
				'Push currency button.',
				'Push number button.'
			]
		],
	]
};

function pickTests(filename, foundTests, config) {

	var coreVersion = config.env.LO_CORE_VERSION;
	var testsToRun = foundTests;
	if (!(coreVersion in testBlackLists))
		return testsToRun;

	var blackList = testBlackLists[coreVersion];
	for (var i = 0; i < blackList.length; i++) {
		if (filename.endsWith(blackList[i][0])) {
			if (blackList[i][1].length === 0) // skip the whole test suite
				return [];
			testsToRun = testsToRun.filter(fullTestName => !blackList[i][1].includes(fullTestName[1]));
		}
	}
	return testsToRun;
}


module.exports = plugin;
