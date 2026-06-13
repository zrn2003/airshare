using System;
using System.IO;
using System.Windows;
using Microsoft.Web.WebView2.Core;
using AirShare.Services;

namespace AirShare.App.Views;

public partial class MainWindow : Window
{
    private readonly ScriptBridge _scriptBridge;

    public MainWindow(SharingService sharingService, DeviceService deviceService)
    {
        InitializeComponent();
        _scriptBridge = new ScriptBridge(sharingService, deviceService);
        InitializeWebView();
    }

    private async void InitializeWebView()
    {
        await webView.EnsureCoreWebView2Async(null);
        
        webView.CoreWebView2.SetVirtualHostNameToFolderMapping(
            "app.local", 
            Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "wwwroot"), 
            CoreWebView2HostResourceAccessKind.Allow);

        string appJs = File.ReadAllText(Path.Combine(AppDomain.CurrentDomain.BaseDirectory, "wwwroot", "app.js"));
        await webView.CoreWebView2.AddScriptToExecuteOnDocumentCreatedAsync(appJs);

        webView.CoreWebView2.AddHostObjectToScript("scriptBridge", _scriptBridge);
        webView.CoreWebView2.Navigate("http://app.local/dashboard.html");
    }
}
