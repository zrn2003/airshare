using System.Globalization;
using System.Windows;
using System.Windows.Data;
using AirShare.App.ViewModels;

namespace AirShare.App.Converters;

public sealed class PageVisibilityConverter : IValueConverter
{
    public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
    {
        if (value is AppPage page && parameter is string pageName &&
            Enum.TryParse<AppPage>(pageName, out var target))
        {
            return page == target ? Visibility.Visible : Visibility.Collapsed;
        }

        return Visibility.Collapsed;
    }

    public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture) =>
        throw new NotSupportedException();
}
