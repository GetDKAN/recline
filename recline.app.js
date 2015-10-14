/**
 * @file
 * Provides options for recline visualization.
 */
(function ($) {
    var maxLabelWidth = 77;
    var labelMargin = 5;

    Drupal.behaviors.Recline = {
        attach: function (context) {
            var delimiter = Drupal.settings.recline.delimiter;
            var file = Drupal.settings.recline.file;
            var uuid = Drupal.settings.recline.uuid;
            var dkan = Drupal.settings.recline.dkan;
            var fileType = Drupal.settings.recline.fileType;
            var dataExplorerSettings = {
                grid: Drupal.settings.recline.grid,
                graph: Drupal.settings.recline.graph,
                map: Drupal.settings.recline.map
            };

            window.dataExplorer = null;
            window.explorerDiv = $('.data-explorer');

            // This is the very basic state collection.
            var state = recline.View.parseQueryString(decodeURIComponent(window.location.hash));
            if ('#map' in state) {
                state.currentView = 'map';
            } else if ('#graph' in state) {
                state.currentView = 'graph';
            } else if ('#timeline' in state) {
                state.currentView = 'timeline';
            }
            // Checks if dkan_datastore is installed.
            if (dkan || fileType == 'text/csv' || fileType == 'csv') {
                var drupal_base_path = Drupal.settings.basePath;
                var DKAN_API = drupal_base_path + 'api/action/datastore/search.json';
                var url = dkan ? (window.location.origin + DKAN_API + '?resource_id=' + uuid) : file;
                var options = dkan ? {} : {delimiter: delimiter};
                var ajax_options = {
                    type: 'GET',
                    url: dkan ? url : file,
                    dataType: dkan  ? 'json' : 'text',
                    beforeSend: function (jqXHR, settings) {
                        /* add url property and get value from settings (or from caturl)*/
                        jqXHR.dkan = dkan;
                        notify({message:'Loading'});
                    },
                    xhrFields: {
                        onprogress: function (e) {
                            if (e.lengthComputable) {
                                notify({message:'Loading', percentage: parseFloat(e.loaded / e.total * 100).toFixed(1) + '%'});
                            } else {
                                console.log('Length not computable.');
                            }
                        }
                    },
                    success: function(data, status, jqXHR) {
                        hideNotification();
                        var dataset, views;
                        if (jqXHR.dkan) {
                            if ('success' in data && data.success) {
                                dataset = new recline.Model.Dataset({
                                    endpoint: window.location.origin + drupal_base_path + '/api',
                                    url: url,
                                    id: uuid,
                                    backend: 'ckan'
                                });
                                dataset.fetch();
                                views = createExplorer(dataset, state, dataExplorerSettings);
                            }
                            else {
                                $('.data-explorer').append('<div class="messages status">Error returned from datastore: ' + data + '.</div>');
                            }
                        }
                        else {
                            // Converts line endings in either format to unix format.
                            data = data.replace(/(\r\n|\n|\r)/gm,"\n");
                            dataset = new recline.Model.Dataset({
                                records: recline.Backend.CSV.parse(data, options)
                            });
                            dataset.fetch();
                            views = createExplorer(dataset, state, dataExplorerSettings);
                            // The map needs to get redrawn when we are delivering from the ajax
                            // call.
                            $.each(views, function(i, view) {
                                if (view.id == 'map') {
                                    view.view.redraw('refresh');
                                }
                            });
                        }
                    }
                };
                if (dkan) {
                    ajax_options.error = function(data, status, jqXHR) {
                        hideNotification();
                        $('.data-explorer').append('<div class="messages status">Unable to connect to the datastore.</div>');
                    };
                }
                else {
                    ajax_options.timeout = Drupal.settings.recline.ajax_timeout;
                    ajax_options.error = function(x, t, m) {
                        hideNotification();
                        if (t === "timeout") {
                            $('.data-explorer').append('<div class="messages status">Viewing this data in your browser is temporally unavailable. Please download and view the data on your computer.</div>');
                        } else {
                            $('.data-explorer').append('<div class="messages status">Viewing this data in your browser is temporally unavailable. Please download and view the data on your computer.</div>');
                        }
                    };
                }
                $.ajax(ajax_options);
            }
            // Checks if xls.
            else if (fileType == 'application/vnd.openxmlformats-officedocument.spreadsheetml.sheet' || fileType == 'application/vnd.ms-excel') {
                var dataset = new recline.Model.Dataset({
                    url: file,
                    backend: 'dataproxy'
                });
                dataset.fetch();
                var views = createExplorer(dataset, state, dataExplorerSettings);
            }
            else {
                $('.data-explorer').append('<div class="messages status">File type ' + fileType + ' not supported for preview.</div>');
            }
        }
    };

    // make Explorer creation / initialization in a function so we can call it
    // again and again
    var createExplorer = function(dataset, state, settings) {
        // remove existing data explorer view
        var reload = false;
        if (window.dataExplorer) {
            window.dataExplorer.remove();
            reload = true;
        }
        window.dataExplorer = null;
        var $el = $('<div />');
        $el.appendTo(window.explorerDiv);

        var views = [];
        var grid, graph, map;

        if (settings.grid) {
            grid = new recline.View.SlickGrid({
                model: dataset
            });
            views.push(
                {
                    id: 'grid',
                    label: 'Grid',
                    view: grid
                }
            );
            grid.listenTo(dataset, 'query:done', function() {
                resizeAllColumns(grid.grid);
            });
        }
        if (settings.graph) {
            state.graphOptions = {
                xaxis: {
                    tickFormatter:tickFormatter(dataset),
                },
                hooks:{
                    processOffset:[processOffset(dataset)],
                    bindEvents: [bindEvents],
                }
            };
            graph = new recline.View.Graph({
                model: dataset,
                state: state
            });
            views.push(
                {
                    id: 'graph',
                    label: 'Graph',
                    view: graph
                }
            );
        }
        if (settings.map) {
            map = new recline.View.Map({
                model: dataset
            });
            views.push(
                {
                    id: 'map',
                    label: 'Map',
                    view: map
                }
            );
        }

        Drupal.settings.recline.args = {
            model: dataset,
            el: $el,
            state: state,
            views: views
        };

        // Getting base embed url.
        var urlBaseEmbed = $('.embed-code').text();
        var iframeOptions = {src: urlBaseEmbed, width:850, height:400};
        // Attaching router to dataexplorer state.
        window.dataExplorer = new recline.View.MultiView(Drupal.settings.recline.args);
        window.router = new recline.DeepLink.Router(window.dataExplorer);

        // Adding router listeners.
        changeEmbedCode = getEmbedCode(iframeOptions);
        window.router.on('init', changeEmbedCode);
        window.router.on('stateChange', changeEmbedCode);

        // Add map dependency just for map views.
        _.each(window.dataExplorer.pageViews, function(item, index){
            if(item.id && item.id === 'map'){
                var map = window.dataExplorer.pageViews[index].view.map;
                window.router.addDependency(new recline.DeepLink.Deps.Map(map, window.router));
            }
        });
        // Start to track state chages.
        window.router.start();

        $.event.trigger('createDataExplorer');
        return views;
    };

    $(".recline-embed a.embed-link").live('click', function(){
      $(this).parents('.recline-embed').find('.embed-code-wrapper').toggle();
      return false;
    });


    //==========================================
    // Functions
    // -----------------------------------------

    function getEmbedCode(options){
        return function(state){
            var iframeOptions = _.clone(options);
            var iframeTmpl = _.template('<iframe width="<%= width %>" height="<%= height %>" src="<%= src %>" frameborder="0"></iframe>');
            _.extend(iframeOptions, {src: iframeOptions.src + '#' + (state.serializedState || '')});
            var html = iframeTmpl(iframeOptions);
            $('.embed-code').text(html);
        };
    }

    function isInverted(){
        return dataExplorer.pageViews[1].view.state.attributes.graphType === 'bars';
    }

    function computeWidth (plot, labels) {
        var biggerLabel = '';
        for( var i = 0; i < labels.length; i++){
            if(labels[i].length > biggerLabel.length && !_.isUndefined(labels[i])){
                biggerLabel = labels[i];
            }
        }
        var canvas = plot.getCanvas();
        var ctx = canvas.getContext('2d');
        ctx.font = 'sans-serif smaller';
        return ctx.measureText(biggerLabel).width;
    }

    function resize (plot) {
        var itemWidth = computeWidth(plot, _.pluck(plot.getXAxes()[0].ticks, 'label'));
        var graph = dataExplorer.pageViews[1];
        if(!isInverted() && $('#prevent-label-overlapping').is(':checked')){
            var canvasWidth = Math.min(itemWidth + labelMargin, maxLabelWidth) * plot.getXAxes()[0].ticks.length;
            var canvasContainerWith = $('.panel.graph').parent().width();
            if(canvasWidth < canvasContainerWith){
                canvasWidth = canvasContainerWith;
            }
            $('.panel.graph').width(canvasWidth);
            $('.recline-flot').css({overflow:'auto'});
        }else{
            $('.recline-flot').css({overflow:'hidden'});
            $('.panel.graph').css({width: '100%'});
        }
        plot.resize();
        plot.setupGrid();
        plot.draw();
    }

    function bindEvents (plot, eventHolder) {
        var p = plot || dataExplorer.pageViews[1].view.plot;
        resize(p);
        setTimeout(addCheckbox, 0);
    }

    function processOffset (dataset) {
        return function(plot, offset) {
            if(dataExplorer.pageViews[1].view.xvaluesAreIndex){
                var series = plot.getData();
                for (var i = 0; i < series.length; i++) {
                      var numTicks = Math.min(dataset.records.length, 200);
                      var ticks = [];
                      for (var j = 0; j < dataset.records.length; j++) {
                        ticks.push(parseInt(j, 10));
                      }
                      if(isInverted()){
                        series[i].yaxis.options.ticks = ticks;
                      }else{
                        series[i].xaxis.options.ticks = ticks;
                      }
                }
            }
        };
    }

    function tickFormatter(dataset){
        return function (x) {
            x = parseInt(x, 10);
            try {
                if(isInverted()){
                    return x;
                }
                var field = dataExplorer.pageViews[1].view.state.get('group');
                var label = dataset.records.models[x].get(field) || "";
                if(!moment(String(label)).isValid() && !isNaN(parseInt(label, 10))){
                    label = parseInt(label, 10) - 1;
                }
                return label;
            } catch(e) {
                return x;
            }
        };
    }

    function addCheckbox() {
        $control = $('.form-stacked:visible').find('#prevent-label-overlapping');
        if(!$control.length){
            $form = $('.form-stacked');
            $checkboxDiv = $('<div class="checkbox"></div>').appendTo($form);
            $label = $('<label />', { 'for': 'prevent-label-overlapping', text: 'Resize graph to prevent label overlapping' }).appendTo($checkboxDiv);
            $label.prepend($('<input />', { type: 'checkbox', id: 'prevent-label-overlapping', value: '' }));
            $control = $('#prevent-label-overlapping');
            $control.on('change', function(){
                resize(dataExplorer.pageViews[1].view.plot);
            });
        }
    }

    function notify(notification) {
        var notification = _.defaults(notification, {percentage: ''});
        var tpl ='<%= message %> <span class="spin">&nbsp;</span> <p><%= percentage %></p>';
        var template = _.template(tpl);
        var $notification_box = $('.loader');

        $notification_box.html(template(notification));
        $notification_box.show();
    }

    function hideNotification(){
        $('.loader').empty().hide();
    }


    // Column resizing based on content width
    // Based on https://github.com/naresh-n/slickgrid-column-data-autosize
    function resizeAllColumns(grid) {
        var $container = $('.recline-data-explorer');
        var elHeaders = $container.find('.slick-header-column');
        var allColumns = grid.getColumns();
        elHeaders.each(function(index, el) {
          var columnDef = $(el).data('column');
          var headerWidth = getElementWidth(el) + 9; // Needed extra right padding
          var colIndex = grid.getColumnIndex(columnDef.id);
          var column = allColumns[colIndex];
          var autoSizeWidth = Math.max(headerWidth, getMaxColumnTextWidth(grid, columnDef, colIndex, $container));
          allColumns[colIndex].width = autoSizeWidth;
        });
        grid.setColumns(allColumns);

        // Adjust width to ensure visibility of horizontal scrollbar in iframe
        // var tableWidth = $('.grid-canvas').outerWidth();
        // $('#ve-table').width(tableWidth);
        grid.setColumns(allColumns);
    }

    function getMaxColumnTextWidth(grid, columnDef, colIndex, $container) {
        var texts = [];
        var rowEl = createRow(columnDef, $container);
        var data = grid.getData();
        for (var i = 0; i < data.getLength(); i++) {
          texts.push(data.getItem(i)[columnDef.field]);
        }
        var template = getMaxTextTemplate(texts, columnDef, colIndex, data, rowEl, $container);
        var width = getTemplateWidth(rowEl, template);
        deleteRow(rowEl);
        return width;
    }

    function getTemplateWidth(rowEl, template) {
        var cell = $(rowEl.find(".slick-cell"));
        cell.append(template);
        $(cell).find("*").css("position", "relative");
        return cell.outerWidth() + 1;
    }

    function getMaxTextTemplate(texts, columnDef, colIndex, data, rowEl, $container) {
        var max = 0,
        maxTemplate = null;
        var formatFun = columnDef.formatter;
        $(texts).each(function(index, text) {
          var template;
          if (formatFun) {
            template = $("<span>" + formatFun(index, colIndex, text, columnDef, data) + "</span>");
            text = template.text() || text;
          }
          var length = text ? getElementWidthUsingCanvas(rowEl, text, $container) : 0;
          if (length > max) {
            max = length;
            maxTemplate = template || text;
          }
        });
        return maxTemplate;
    }

    function createRow(columnDef, $container) {
        var rowEl = $('<div class="slick-row"><div class="slick-cell"></div></div>');
        rowEl.find('.slick-cell').css({
          'visibility': 'hidden',
          'text-overflow': 'initial',
          'white-space': 'nowrap'
        });
        var gridCanvas = $container.find('.grid-canvas');
        $(gridCanvas).append(rowEl);
        return rowEl;
    }

    function deleteRow(rowEl) {
        $(rowEl).remove();
    }

    function getElementWidth(element) {
        var width, clone = element.cloneNode(true);
        clone.style.cssText = 'position: absolute; visibility: hidden;right: auto;text-overflow: initial;white-space: nowrap;';
        element.parentNode.insertBefore(clone, element);
        width = clone.offsetWidth;
        clone.parentNode.removeChild(clone);
        return width;
    }

    function getElementWidthUsingCanvas(element, text) {
        var context = document.createElement("canvas").getContext("2d");
        context.font = element.css("font-size") + " " + element.css("font-family");
        var metrics = context.measureText(text);
        return metrics.width;
    }

})(jQuery);
