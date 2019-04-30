// Config file for running Rollup in "normal" mode (non-watch)

// import rollupGitVersion from 'rollup-plugin-git-version'
// import json from 'rollup-plugin-json'
// import gitRev from 'git-rev-sync'
// import pkg from './package.json'
import { uglify } from 'rollup-plugin-uglify'
import postcss from 'rollup-plugin-postcss'
import resolve from 'rollup-plugin-node-resolve'
import commonjs from 'rollup-plugin-commonjs'
import legacy from 'rollup-plugin-legacy'


// let {version} = pkg;
let release;

// Check env variables, in order to uglify (minify) for a non-debug build
if (process.env.NODE_ENV === 'release') {
	release = true;
} else {
	release = false;
}

const banner = `/* @preserve
 * This file is part of the LibreOffice project.
 *
 * See https://cgit.freedesktop.org/libreoffice/online/
 */
`;

const rollup_plugins = [
	// The resolve plugin makes Rollup look into node_modules for imports
	resolve(),

	// The postCSS rollup plugin takes care of concatenating all import()ed 
	// CSS files together
	postcss({
		inject: false,
		
		// Filename to write all styles to
		extract: 'dist/loleaflet-bundled.css',
		 
		sourceMap: !release
	}),

	// w2utils uses globals instead of using any kind of module loader.
	// Using rollup-plugin-legacy allows main.js to import and re-expose these as globals again.
	legacy({
		'js/w2ui-1.5.rc1.js': {
			'w2utils' : 'w2utils',
			'w2ui' : 'w2ui'
		},
	}),
	
	commonjs({
		include: ['node_modules/@braintree/sanitize-url/**']
	})
];


if (release) {
	// For non-debug builds, add the uglify plugin in order to minify the bundle.
	rollup_plugins.push(
		uglify()
	);
}

export default {
	input: 'src/loleaflet.js',
	
	// The 'autolinker' module is wrapped within a IIFE that assumes 
	// 'this' is the global 'window'.
	moduleContext: { 
		'node_modules/autolinker/dist/Autolinker.js': 'window',
		'node_modules/jquery/dist/jquery.js': 'undefined'
	},
	
	output: [
		{
			file: "dist/loleaflet-bundled.js",
			format: 'umd',
			name: 'L',
			banner: banner,
// 			outro: outro,
			sourcemap: !release,
		},
// 		{
// 			file: 'dist/loleaflet-bundled.esm.js',
// 			format: 'es',
// 			banner: banner,
// 			sourcemap: true
// 		}
	],
	plugins: rollup_plugins
};
