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
	],

	'cp-6-4': [
		['mobile/calc/apply_font_spec.js',
			[
				'Apply font size.'
			]
		],
		['mobile/writer/apply_font_spec.js',
			[
				'Apply font size.'
			]
		],
		['mobile/impress/apply_font_spec.js',
			[]
		],
		['mobile/impress/apply_font_spec.js',
			[]
		],
		['mobile/impress/apply_paragraph_props_spec.js',
			[]
		],
		['mobile/writer/table_properties_spec.js',
			[]
		],
	]
};

module.exports.testBlackLists = testBlackLists;
