package com.mii.app

import androidx.annotation.StringRes
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.rememberScrollState
import androidx.compose.foundation.verticalScroll
import androidx.compose.material3.Card
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.NavigationBar
import androidx.compose.material3.NavigationBarItem
import androidx.compose.material3.Scaffold
import androidx.compose.material3.Text
import androidx.compose.material3.lightColorScheme
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableIntStateOf
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.tooling.preview.Preview
import androidx.compose.ui.unit.dp

private enum class Destination(@StringRes val label: Int, val symbol: String) {
    Device(R.string.device, "◎"), Design(R.string.design, "▧"),
    Data(R.string.data, "≋"), Settings(R.string.settings, "⋯"),
}

@Composable
fun MiiApp() {
    var selected by rememberSaveable { mutableIntStateOf(0) }
    MaterialTheme(colorScheme = lightColorScheme(primary = Color(0xFF276B57), background = Color(0xFFF7F8F5))) {
        Scaffold(bottomBar = {
            NavigationBar {
                Destination.entries.forEachIndexed { index, destination ->
                    NavigationBarItem(
                        selected = index == selected, onClick = { selected = index },
                        icon = { Text(destination.symbol) }, label = { Text(stringResource(destination.label)) },
                    )
                }
            }
        }) { insets ->
            Column(
                modifier = Modifier.fillMaxSize().padding(insets).verticalScroll(rememberScrollState()).padding(24.dp),
                verticalArrangement = Arrangement.spacedBy(20.dp),
            ) {
                Text("MII", style = MaterialTheme.typography.headlineLarge)
                Text(stringResource(R.string.tagline), style = MaterialTheme.typography.bodyMedium)
                when (Destination.entries[selected]) {
                    Destination.Device -> EmptyCard(R.string.unbound_title, R.string.unbound_body)
                    Destination.Design -> EmptyCard(R.string.templates_empty, R.string.templates_body)
                    Destination.Data -> EmptyCard(R.string.records_empty, R.string.records_body)
                    Destination.Settings -> {
                        EmptyCard(R.string.local_title, R.string.local_body)
                        Text(stringResource(R.string.version, BuildConfig.VERSION_NAME))
                    }
                }
            }
        }
    }
}

@Composable
private fun EmptyCard(@StringRes title: Int, @StringRes body: Int) {
    Card(modifier = Modifier.fillMaxWidth()) {
        Column(Modifier.padding(20.dp), verticalArrangement = Arrangement.spacedBy(12.dp)) {
            Text(stringResource(title), style = MaterialTheme.typography.titleLarge)
            Text(stringResource(body), style = MaterialTheme.typography.bodyLarge)
        }
    }
}

@Preview(showBackground = true, widthDp = 360, heightDp = 760)
@Composable
private fun MiiPreview() { MiiApp() }
