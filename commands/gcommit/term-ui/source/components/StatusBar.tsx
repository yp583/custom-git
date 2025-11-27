import React from 'react';
import {Text, Box} from 'ink';

export default function StatusBar() {
	return (
		<Box marginTop={1}>
			<Text dimColor>
				Press <Text bold>q</Text> to continue with commits · <Text bold>c</Text> to cancel
			</Text>
		</Box>
	);
}
