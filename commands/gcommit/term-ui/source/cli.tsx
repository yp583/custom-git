#!/usr/bin/env node
import React from 'react';
import {render} from 'ink';
import fs from 'fs';
import App from './app.js';

const VIZ_PATH = '/tmp/patches/visualization.json';

// Read visualization data
let data: {points: any[]; clusters: any[]};
try {
	const content = fs.readFileSync(VIZ_PATH, 'utf-8');
	data = JSON.parse(content);
} catch (err) {
	console.error(`Error reading ${VIZ_PATH}:`, err);
	process.exit(1);
}

let exitCode = 0;

const {waitUntilExit} = render(
	<App
		points={data.points}
		clusters={data.clusters}
		onContinue={() => {
			exitCode = 0;
		}}
		onCancel={() => {
			exitCode = 1;
		}}
	/>,
);

await waitUntilExit();
process.exit(exitCode);
